#pragma once
#include <ceddec/types.hpp>

namespace ceddec {
    class CEDDEC_API IRLifter {
    public:
        static std::vector<IRInstruction> LiftInstruction(const ParsedInstruction& parsed_inst);

        static ControlFlowGraph BuildCFG(const std::vector<ParsedInstruction>& instructions);
        static void ApplyLocalSSA(ControlFlowGraph& cfg);

        static void CalculateDominators(ControlFlowGraph& cfg);
        static void CalculateDominanceFrontiers(ControlFlowGraph& cfg);

        static void InsertPhiNodes(ControlFlowGraph& cfg);
        static void RenameVariables(ControlFlowGraph& cfg);
    };
}
