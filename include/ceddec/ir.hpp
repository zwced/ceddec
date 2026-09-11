#pragma once
#include <ceddec/types.hpp>
#include <set>
#include <map>

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

        /*
         * registers that are read on some path from the entry block before
         * being written on that same path anywhere in the function
         * i.e. registers whose value flows in from the caller.
         * This is the single 'source of truth' for "is this register a parameter", used by both
         * the calling-convention analyzer (for the function signature) and
         * the AST builder (for naming inside the body), so the two can never
         * disagree with each other.
         */
        static std::set<Register> FindUpwardExposedRegisters(const ControlFlowGraph& cfg);

        /*
         * collapses a register to its 64-bit "family" member, e.g. EDI/DI/DIL -> RDI,
         * so different-width views of the same physical argument register match up
         */
        static Register RegisterFamily64(Register r);

        /* SysV AMD64 integer/pointer argument-passing order */
        static const std::vector<Register>& SysVIntArgOrder();

        /*
         * maps each upward-exposed register (collapsed to its 64-bit family) to its
         * 0-based position in SysV argument order, in the order params actually appear
         */
        static std::map<Register, size_t> BuildParamIndexMap(const std::set<Register>& live_in);

        /* true iff the CFG edge from_block -> to_block is a loop back edge */
        static bool IsBackEdge(const ControlFlowGraph& cfg, size_t from_block, size_t to_block);
    };
}
