#pragma once
#include <ceddec/types.hpp>
#include <source/misc/register_name.hpp>
#include <string>
#include <functional>

namespace ceddec {
    /*
     * naming policy for decompiled output. Every "what should this thing be called in the emitted C" decision goes through here,
     * so the whole naming scheme can be swapped without touching ast_builder.cpp or calling_conv.hpp.
     */
    struct NamingScheme {
        /* nth incoming register parameter (0-indexed internally). Default matches IDA Pro: a1, a2, a3, ... */
        std::function<std::string(size_t index)> param_name = [](size_t index) { return "a" + std::to_string(index + 1); };

        /* base name only for a register that is NOT a parameter  */
        std::function<std::string(Register reg, uint32_t ver)> local_name = [](Register reg, uint32_t /*ver*/) {
            return GetRegisterName(reg);
        };

        /* a stack-frame local at `abs_offset` from rbp/rsp that ISN'T a recognized param slot */
        std::function<std::string(int64_t abs_offset)> stack_local_name = [](int64_t abs_offset) { return "var_" + std::to_string(abs_offset); };

        /* the decompiled function's own name */
        std::function<std::string()> function_name = []() { return "sub_function"; };
    };

    /*
     * process-wide active scheme; swap fields on it before decompiling to
     * change how everything downstream (signature + body) is named.
     */
    CEDDEC_API NamingScheme& ActiveNamingScheme();
}
