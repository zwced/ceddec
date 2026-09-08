#include <ceddec/types.hpp>
#include <ceddec/ast.hpp>

namespace ceddec {
    inline static FunctionSignature AnalyzeCallingConvention(const ControlFlowGraph& cfg) {
        FunctionSignature sig;
        sig.name = "sub_function";
        sig.return_type = "int64_t";

        bool uses_edi = false;
        bool uses_esi = false;

        for (const auto& block : cfg.blocks) {
            for (const auto& inst : block.instructions) {
                auto check_op = [&](const IROperand& op) {
                    if (std::holds_alternative<Register>(op.value)) {
                        Register r = std::get<Register>(op.value);
                        if (r == Register::EDI || r == Register::RDI) uses_edi = true;
                        if (r == Register::ESI || r == Register::RSI) uses_esi = true;
                    } else if (std::holds_alternative<std::string>(op.value)) {
                        std::string s = std::get<std::string>(op.value);
                        if (s == "edi" || s == "rdi") uses_edi = true;
                        if (s == "esi" || s == "rsi") uses_esi = true;
                    }
                };
                check_op(inst.source);
                check_op(inst.destination);
            }
        }

        if (uses_edi) sig.params.push_back("int64_t arg0");
        if (uses_esi) sig.params.push_back("int64_t arg1");

        return sig;
    }
}
