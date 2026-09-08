#pragma once
#include <ceddec/types.hpp>
#include <ceddec/ir.hpp>

namespace ceddec {
    CEDDEC_API void PrintParsedBlock(const char* title, const ParsedBlock& block);
    CEDDEC_API void PrintCFGAnalysis(const ControlFlowGraph& cfg);
}
