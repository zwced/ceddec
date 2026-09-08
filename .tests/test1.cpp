#include <ceddec/parser.hpp>
#include <ceddec/types.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/printer.hpp>
#include <cstdio>

using namespace ceddec;

int main() {
    Parser intel64 = CreateParser({Architecture::x86_64, true, true, true});
    ParsedBlock intel_blk = intel64->ParseBlock(R"(
    cmp rax, 0x10
    jge .L2
    add rax, 0x1
    jmp .L3
.L2:
    sub rax, 0x1
.L3:
    mov rbx, rax
    )");
    PrintParsedBlock("Intel x86_64", intel_blk);

    ControlFlowGraph cfg = IRLifter::BuildCFG(intel_blk.instructions);

    IRLifter::CalculateDominators(cfg);
    IRLifter::CalculateDominanceFrontiers(cfg);
    IRLifter::InsertPhiNodes(cfg);

    IRLifter::RenameVariables(cfg);

    PrintCFGAnalysis(cfg);

    return 0;
}
