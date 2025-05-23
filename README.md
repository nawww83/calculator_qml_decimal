# Калькулятор Decimal 128-бит

## Поддерживает

 * Разделитель тысяч
 * Регулируемое количество знаков после запятой от 0 до 12
 * Историю операций
 * Отображение текущей операции и первого операнда во вспомогательном поле
 * Повторное выполнение введенной операции, в т.ч. и при длительном нажатии Enter
 * Горячее редактирование текущего числа (корректировка) курсором ввода
 * Максимальное абсолютное значение целой части $\{2}^{128} - 1$, вводится кнопкой "Max Int"
 * Для отрисовки окон используется технология QML.

Скачать последнюю версию для Windows https://disk.yandex.ru/d/FcqdjAe5ofcNHQ

## Особенности работы с калькулятором

 * Для смены знака числа нажмите "Underscore": Shift + '-'
 * Количество знаков после запятой устанавливается в окне через Ctrl + 'S'
 * При нажатии Esc (или кнопки "C") калькулятор полностью сбрасывается
 * Представлены базовые операции: сложение, вычитание, умножение и деление. 
 * Работает правило округления всех девяток (версия после 1.02).

## Изменения относительно версии 1.00

### Версия 1.01, 21.04.2025

 * Добавлена операция извлечения квадратного корня.
 * Поправлена ошибка умножения, когда оба операнда отрицательные и по модулю больше единицы.

### Версия 1.02, 27.04.2025

- Добавлены операции
    - Квадрат числа.
    - Обратное число, 1/x.
- Добавлена факторизация целого числа на простые множители. Стоп по кнопке "Сброс".
- Улучшен интерфейс.

### Версия 1.03, 18.05.2025

* Добавлена возможность сохранения результата в память
* Добавлено округление всех девяток для более корректного счета.

## Другое

Сборка приложения проверялась под Qt 6.6.2, Windows 11.
