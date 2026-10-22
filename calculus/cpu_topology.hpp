/**
 * @file cpu_topology.hpp
 * @brief Определение рабочих потоков и их привязка к P-ядрам.
 * @details На гибридных процессорах (Intel Alder Lake и новее, ARM64 Windows,
 *          Apple Silicon) ядра делятся на производительные (P, performance)
 *          и энергоэффективные (E, efficiency). Для ресурсоёмких вычислений
 *          (SIQS, ECM) нужны только P-ядра: E-ядра в разы медленнее и лишь
 *          добавляют синхронизацию и борьбу за кэш.
 *          Поэтому число потоков берётся равным числу логических процессоров
 *          P-ядер, а сами потоки жёстко закрепляются на них (affinity), чтобы
 *          планировщик не сдвинул часть работы на E-ядра, оставив P-ядра
 *          простаивать в ожидании отставших.
 *          Если гибридность не обнаружена или ОС не даёт о ней сведений,
 *          используется обычное std::thread::hardware_concurrency() без привязки.
 */

#pragma once

#include <cstdint>
#include <thread>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__)
#  include <algorithm>
#  include <filesystem>
#  include <fstream>
#  include <pthread.h>
#  include <sched.h>
#  include <string>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#endif

namespace u128::cpu
{

// Внимание: идентификатор slots здесь не используется — в заголовках Qt это
// макрос (раскрывается в пустую строку), и он ломает объявления переменных.

/**
 * @brief Логический процессор, на который можно посадить рабочий поток.
 * @details Помимо системного номера хранит платформенные координаты, нужные
 *          для установки affinity (на Windows логические процессоры
 *          адресуются парой «группа + бит в маске»).
 */
struct CpuSlot
{
    unsigned index; ///< Системный номер логического процессора.

#if defined(_WIN32)
    unsigned short group; ///< Номер группы процессоров.
    unsigned char bit;    ///< Позиция в маске группы.
#endif
};

namespace detail
{

#if defined(_WIN32)

/**
 * @brief Класс энергоэффективности физического ядра.
 * @details 0 — P-ядро (максимальная производительность), больше 0 — E-ядро.
 *          В актуальных Windows SDK поле называется EfficiencyClass, в MinGW-w64
 *          его нет, но оно занимает первый байт Reserved (сразу за Flags).
 */
template <typename Rel>
inline unsigned efficiency_class(const Rel &relationship)
{
    if constexpr (requires { relationship.EfficiencyClass; })
        return relationship.EfficiencyClass;
    else
        return relationship.Reserved[0];
}

/**
 * @brief Список логических процессоров P-ядер (Windows).
 * @details Каждое отношение RelationProcessorCore описывает одно физическое
 *          ядро и маску его логических процессоров. Ядра с EfficiencyClass == 0
 *          считаются производительными; E-ядра игнорируются. На негибридных
 *          системах класс у всех ядер нулевой, поэтому в список попадают все
 *          логические процессоры (поведение как у hardware_concurrency()).
 */
inline std::vector<CpuSlot> performance_slots_win32()
{
    DWORD buffer_size = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &buffer_size))
        return {};
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return {};

    // Топология может измениться между вызовами, поэтому повторяем при нехватке буфера.
    std::vector<unsigned char> buffer;
    bool ok = false;
    for (int attempt = 0; attempt < 4 && !ok; ++attempt)
    {
        buffer.resize(buffer_size);
        ok = GetLogicalProcessorInformationEx(
                 RelationProcessorCore,
                 reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                 &buffer_size)
             != FALSE;

        if (!ok && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return {};
    }

    if (!ok)
        return {};

    // Системные номера процессоров сквозные по группам, поэтому нужны смещения.
    std::vector<DWORD> group_base;
    const DWORD groups = GetActiveProcessorGroupCount();
    group_base.reserve(groups);
    DWORD next_index = 0;
    for (DWORD g = 0; g < groups; ++g)
    {
        group_base.push_back(next_index);
        next_index += GetActiveProcessorCount(static_cast<WORD>(g));
    }

    std::vector<CpuSlot> cpus;
    DWORD offset = 0;
    while (offset < buffer_size)
    {
        const auto *info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
            buffer.data() + offset);

        if (info->Size == 0)
            break;

        if (info->Relationship == RelationProcessorCore && efficiency_class(info->Processor) == 0)
        {
            for (WORD g = 0; g < info->Processor.GroupCount; ++g)
            {
                const GROUP_AFFINITY &mask = info->Processor.GroupMask[g];
                if (mask.Group >= group_base.size())
                    continue;

                for (unsigned bit = 0; bit < sizeof(KAFFINITY) * 8; ++bit)
                {
                    const KAFFINITY flag = static_cast<KAFFINITY>(1) << bit;
                    if ((mask.Mask & flag) == 0)
                        continue;

                    cpus.push_back(CpuSlot{group_base[mask.Group] + bit,
                                            static_cast<unsigned short>(mask.Group),
                                            static_cast<unsigned char>(bit)});
                }
            }
        }

        offset += info->Size;
    }

    return cpus;
}

inline bool pin_current_thread_to_slot(const CpuSlot &slot)
{
    GROUP_AFFINITY affinity{};
    affinity.Group = slot.group;
    affinity.Mask = static_cast<KAFFINITY>(1) << slot.bit;
    return SetThreadGroupAffinity(GetCurrentThread(), &affinity, nullptr) != FALSE;
}

#elif defined(__linux__)

inline std::string read_line(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line))
        return {};

    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '
                             || line.back() == '\t'))
    {
        line.pop_back();
    }
    return line;
}

inline unsigned parse_first_unsigned(const std::string &text)
{
    size_t pos = text.find_first_of("0123456789");
    if (pos == std::string::npos)
        return 0;

    unsigned value = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
    {
        value = value * 10 + static_cast<unsigned>(text[pos] - '0');
        ++pos;
    }
    return value;
}

/**
 * @brief Процессоры, разрешённые текущему процессу (cgroup/cpuset).
 * @details Читает Cpus_allowed_list из /proc/self/status. Пустой результат
 *          означает «ограничений нет или узнать не удалось».
 */
inline std::vector<bool> allowed_cpus_linux()
{
    std::ifstream in("/proc/self/status");
    std::string line;
    const std::string key = "Cpus_allowed_list:";

    while (std::getline(in, line))
    {
        if (line.compare(0, key.size(), key) != 0)
            continue;

        std::vector<bool> allowed;
        const std::string list = line.substr(key.size());

        size_t pos = 0;
        while (pos < list.size())
        {
            size_t comma = list.find(',', pos);
            if (comma == std::string::npos)
                comma = list.size();

            const std::string item = list.substr(pos, comma - pos);
            const size_t dash = item.find('-');

            unsigned first = parse_first_unsigned(item);
            unsigned last = first;
            if (dash != std::string::npos)
                last = parse_first_unsigned(item.substr(dash + 1));

            if (first > last)
                break;
            if (last > 4095) // защита от мусора во входных данных
                last = 4095;

            if (allowed.size() <= last)
                allowed.resize(last + 1, false);
            for (unsigned cpu = first; cpu <= last; ++cpu)
                allowed[cpu] = true;

            pos = comma + 1;
        }

        return allowed;
    }

    return {};
}

/**
 * @brief Список логических процессоров P-ядер (Linux).
 * @details В sysfs нет прямого признака P/E, поэтому P-ядра определяются по
 *          верхнему уровню максимальной частоты (cpuinfo_max_freq). На
 *          негибридных системах частоты одинаковы, и в список попадают все
 *          доступные процессоры. Если cpufreq недоступен (ВМ, контейнер),
 *          список пуст и вызывающая сторона откатывается на hardware_concurrency().
 */
inline std::vector<CpuSlot> performance_slots_linux()
{
    namespace fs = std::filesystem;

    const fs::path cpu_root{"/sys/devices/system/cpu"};
    std::error_code ec;
    fs::directory_iterator it(cpu_root, ec);
    const fs::directory_iterator end;

    struct Entry
    {
        unsigned cpu;
        unsigned max_freq;
    };

    std::vector<Entry> entries;
    unsigned max_freq = 0;

    for (; !ec && it != end; it.increment(ec))
    {
        const std::string name = it->path().filename().string();
        if (name.size() <= 3 || name.compare(0, 3, "cpu") != 0)
            continue;
        if (!std::all_of(name.begin() + 3, name.end(),
                         [](char ch) { return ch >= '0' && ch <= '9'; }))
        {
            continue;
        }

        const unsigned freq =
            parse_first_unsigned(read_line(it->path() / "cpufreq" / "cpuinfo_max_freq"));
        if (freq == 0)
            continue;

        entries.push_back({parse_first_unsigned(name.substr(3)), freq});
        max_freq = std::max(max_freq, freq);
    }

    if (entries.empty() || max_freq == 0)
        return {};

    // Верхние 5% частотного диапазона — P-ядра (разрыв с E-ядрами заметно больше).
    const unsigned threshold = max_freq - max_freq / 20;
    const std::vector<bool> allowed = allowed_cpus_linux();

    std::vector<CpuSlot> cpus;
    cpus.reserve(entries.size());
    for (const auto &entry : entries)
    {
        if (entry.max_freq < threshold)
            continue;
        if (!allowed.empty() && (entry.cpu >= allowed.size() || !allowed[entry.cpu]))
            continue;

        cpus.push_back(CpuSlot{entry.cpu});
    }

    std::sort(cpus.begin(), cpus.end(),
              [](const CpuSlot &lhs, const CpuSlot &rhs) { return lhs.index < rhs.index; });
    return cpus;
}

inline bool pin_current_thread_to_slot(const CpuSlot &slot)
{
    if (slot.index >= CPU_SETSIZE)
        return false;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(slot.index, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

#endif

} // namespace detail

/**
 * @brief Логические процессоры P-ядер, на которых имеет смысл считать.
 * @details Список вычисляется один раз и кэшируется. Пустой список означает,
 *          что гибридность/топология не определена (например, macOS, где
 *          жёсткой привязки потоков нет) — тогда используется обычный счётчик.
 */
inline const std::vector<CpuSlot> &performance_cpu_slots()
{
#if defined(_WIN32)
    static const std::vector<CpuSlot> cpus = detail::performance_slots_win32();
#elif defined(__linux__)
    static const std::vector<CpuSlot> cpus = detail::performance_slots_linux();
#else
    static const std::vector<CpuSlot> cpus;
#endif
    return cpus;
}

/**
 * @brief Количество логических процессоров P-ядер.
 * @return Число потоков на производительных ядрах или 0, если гибридность
 *         не определена (тогда нужен откат на hardware_concurrency()).
 */
inline unsigned performance_thread_count()
{
    const auto &cpus = performance_cpu_slots();
    if (!cpus.empty())
        return static_cast<unsigned>(cpus.size());

#if defined(__APPLE__)
    // На Apple Silicon hw.perflevel0 — производительный уровень (P-ядра).
    // На Intel Mac ключа нет, и остаётся общий счётчик.
    unsigned value = 0;
    size_t size = sizeof(value);
    if (sysctlbyname("hw.perflevel0.logicalcpu", &value, &size, nullptr, 0) == 0 && value > 0)
        return value;
    return 0;
#else
    return 0;
#endif
}

/**
 * @brief Количество потоков для ресурсоёмких вычислений (SIQS, ECM).
 * @details Учитываются только P-ядра; E-ядра игнорируются.
 */
inline unsigned worker_thread_count()
{
    unsigned count = performance_thread_count();
    if (count == 0)
        count = std::thread::hardware_concurrency();
    if (count == 0)
        count = 4;
    return count;
}

/**
 * @brief Закрепить текущий поток за его P-ядром.
 * @param thread_index Порядковый номер рабочего потока.
 * @return true, если привязка установлена.
 * @details Потоков ровно столько же, сколько процессоров в списке, поэтому
 *          каждый получает свой. Ошибки привязки (например, процесс ограничен
 *          cpuset) не фатальны: поток просто продолжит работу без неё.
 */
inline bool pin_current_thread(unsigned thread_index)
{
    const auto &cpus = performance_cpu_slots();
    if (cpus.empty())
        return false;

    return detail::pin_current_thread_to_slot(cpus[thread_index % cpus.size()]);
}

} // namespace u128::cpu
