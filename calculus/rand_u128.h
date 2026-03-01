#pragma once

#include <chrono>
#include <random>
#include "random_gen.h"

namespace u128_rand {

struct RandomGenerator {

    static auto get_random_u32x4(int64_t offset) {
        const int64_t since_epoch_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::system_clock::now().time_since_epoch()).count();
        std::seed_seq g_rnd_sequence{since_epoch_ms & 255, (since_epoch_ms >> 8) & 255,
                                     (since_epoch_ms >> 16) & 255, (since_epoch_ms >> 24) & 255, offset};
        lfsr8::u32x4 st;
        g_rnd_sequence.generate(st.begin(), st.end());
        return st;
    }

    explicit RandomGenerator() {
        lfsr8::u64 tmp;
        mGenerator.seed(get_random_u32x4((lfsr8::u64)&tmp));
    }
    lfsr_rng_2::gens mGenerator;
};
}
