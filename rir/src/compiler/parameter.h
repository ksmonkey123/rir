#ifndef PIR_PARAMETER_H
#define PIR_PARAMETER_H

#include <stddef.h>

namespace rir {

enum class ReflectGuard : unsigned { None = 0, Warn = 1, Error = 2 };

namespace pir {

struct Parameter {
    static bool DEBUG_DEOPTS;
    static bool DEOPT_CHAOS;
    static bool DEOPT_CHAOS_SEED;
    static size_t MAX_INPUT_SIZE;
    static unsigned RIR_WARMUP;
    static unsigned DEOPT_ABANDON;

    static size_t INLINER_MAX_SIZE;
    static size_t INLINER_MAX_INLINEE_SIZE;
    static size_t INLINER_INITIAL_FUEL;
    static size_t INLINER_INLINE_UNLIKELY;

    static bool RIR_PRESERVE;
    static unsigned RIR_SERIALIZE_CHAOS;

    static unsigned RIR_CHECK_PIR_TYPES;
    static ReflectGuard RIR_REFLECT_GUARD;
};
} // namespace pir
} // namespace rir

#endif
