#pragma once
#include <ceddec/types.hpp>
#include <ceddec/ast.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/naming.hpp>

namespace ceddec {
    inline static FunctionSignature AnalyzeCallingConvention(const ControlFlowGraph& cfg) {
        FunctionSignature sig;
        sig.name = ActiveNamingScheme().function_name();
        sig.return_type = "int64_t";

        /*
         * reuses the exact same upward-exposed-register analysis the AST
         * builder uses, so the function signature and the body can never
         * disagree about which registers are parameters or what order
         * they're in.
         */
        std::set<Register> live_in = IRLifter::FindUpwardExposedRegisters(cfg);
        std::map<Register, size_t> param_index = IRLifter::BuildParamIndexMap(live_in);

        sig.params.resize(param_index.size());
        for (const auto& [reg, idx] : param_index) {
            sig.params[idx] = "int64_t " + ActiveNamingScheme().param_name(idx);
        }

        return sig;
    }
}
