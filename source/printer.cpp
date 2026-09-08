#include <ceddec/printer.hpp>
#include <cstdio>

namespace ceddec {
    static const char* OpName(OperandType t) {
        switch (t) {
            case OperandType::Register:  return "reg";
            case OperandType::Memory:    return "mem";
            case OperandType::Immediate: return "imm";
            case OperandType::Label:     return "label";
            default:                     return "unk";
        }
    }

    void PrintParsedBlock(const char* title, const ParsedBlock& block) {
        printf("| %s |\n", title);
        for (const auto& in : block.instructions) {
            printf("  %.*s", (int)in.mnemonic.size(), in.mnemonic.data());
            for (const auto& op : in.operands) {
                printf(" [%s: %.*s]", OpName(op.type), (int)op.raw_text.size(), op.raw_text.data());
            }
            printf("\n");
        }
        printf("\n");
    }

    void PrintCFGAnalysis(const ControlFlowGraph& cfg) {
        printf("| CFG Dominator & Global SSA Analysis |\n");
        for (const auto& block : cfg.blocks) {
            printf("  Block %zu:\n", block.id);

            printf("    Predecessors: ");
            for (size_t p : block.predecessors) printf("%zu ", p);
            printf("\n");

            printf("    Dominators: ");
            for (size_t d : block.dominators) printf("%zu ", d);
            printf("\n");

            if (block.idom != static_cast<size_t>(-1)) {
                printf("    Immediate Dom: %zu\n", block.idom);
            } else {
                printf("    Immediate Dom: NONE\n");
            }

            printf("    Dom Frontier: ");
            for (size_t f : block.dom_frontier) printf("%zu ", f);
            printf("\n");

            printf("    Instructions:\n");
            for (const auto& inst : block.instructions) {
                if (inst.opcode == IROpcode::Phi) {
                    printf("      [PHI] Dest_v%u = Phi(", inst.destination.ssa_version);
                    for (size_t i = 0; i < inst.phi_sources.size(); ++i) {
                        printf("v%u%s", inst.phi_sources[i].ssa_version,
                               (i + 1 < inst.phi_sources.size()) ? ", " : "");
                    }
                    printf(")\n");
                } else {
                    printf("      [Opcode %d]", (int)inst.opcode);
                    if (std::holds_alternative<Register>(inst.destination.value)) {
                        printf(" | Dest_v%u", inst.destination.ssa_version);
                    }
                    if (std::holds_alternative<Register>(inst.source.value)) {
                        printf(" | Src_v%u", inst.source.ssa_version);
                    }
                    printf("\n");
                }
            }
            printf("\n");
        }
    }
}
