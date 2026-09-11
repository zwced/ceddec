#include <ceddec/parser.hpp>
#include <ceddec/types.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/ast.hpp>
#include <ceddec/calling_conv.hpp>
#include <ceddec/formatter.hpp>
#include <ceddec/naming.hpp>
#include <ceddec/emitters/c.hpp>
#include <ceddec/printer.hpp>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

using namespace ceddec;

namespace {
    struct TestCase {
        std::string name;
        std::string asm_text;
        bool intel_syntax = true;
    };

    /*
    * runs one test case through the full pipeline: parse -> CFG -> dominators ->
    * phi insertion -> SSA rename -> AST -> C emission. Never throws out of this
    * function; a failure at any stage is reported and the runner moves on to the
    * next test case instead of aborting the whole run.
    */
    void RunTestCase(const TestCase& tc) {
        std::cout << "\n|> " << tc.name << " |\n";

        try {
            Parser parser = CreateParser({
                .arch = Architecture::x86_64,
                .intel_syntax = tc.intel_syntax,
                .strict_mode = true,
                .default_hex_immediates = false
            });

            ParsedBlock parsed_blk = parser->ParseBlock(tc.asm_text);

            if (parsed_blk.instructions.empty()) {
                std::cout << "[FAIL] no instructions parsed\n";
                return;
            }

    #if __has_include(<ceddec/printer.hpp>)
            PrintParsedBlock("", parsed_blk);
    #endif

            ControlFlowGraph cfg = IRLifter::BuildCFG(parsed_blk.instructions);

            IRLifter::CalculateDominators(cfg);
            IRLifter::CalculateDominanceFrontiers(cfg);
            IRLifter::InsertPhiNodes(cfg);
            IRLifter::RenameVariables(cfg);

            std::cout << "[+] " << cfg.blocks.size() << " basic block(s)\n";

            std::shared_ptr<ASTBlockStmt> ast_tree = ASTBuilder::BuildAST(cfg);
            if (!ast_tree) {
                std::cout << "[FAIL] AST construction returned null\n";
                return;
            }

            FunctionSignature sig = AnalyzeCallingConvention(cfg);
            std::string header = FormatFunctionHeader(sig);

            CEmitter emitter;
            std::string emitted = emitter.EmitFunction(header, sig.return_type, *ast_tree);

            std::cout << emitted;

        } catch (const std::exception& e) {
            std::cout << "[FAIL] exception: " << e.what() << "\n";
        } catch (...) {
            std::cout << "[FAIL] unknown exception\n";
        }
    }

    std::vector<TestCase> BuildTestCases() {
        return {
            {
                "straightline_params",
                R"(
                    mov eax, edi
                    add eax, esi
                    ret
                )"
            },
            {
                "branch_max",
                R"(
                    mov eax, edi
                    cmp eax, esi
                    jge skip
                    mov eax, esi
                skip:
                    ret
                )"
            },
            {
                "loop_sum",
                R"(
                    mov eax, 0
                    mov ecx, 0
                loop_top:
                    cmp ecx, edi
                    jge loop_end
                    add eax, ecx
                    inc ecx
                    jmp loop_top
                loop_end:
                    ret
                )"
            },
            {
                "call_forwarding",
                R"(
                    mov eax, edi
                    add eax, 1
                    call helper
                    add eax, 1
                    ret
                )"
            },
            {
                "stack_spill_roundtrip",
                R"(
                    mov dword ptr [rbp-4], edi
                    mov eax, dword ptr [rbp-4]
                    add eax, 1
                    mov dword ptr [rbp-4], eax
                    mov eax, dword ptr [rbp-4]
                    ret
                )"
            },
            {
                /* mirrors the original example: signed comparison branch + fallthrough merge */
                "signed_branch_merge_intel",
                R"(
                    cmp rax, 0x10
                    jge .L2
                    add rax, 0x1
                    jmp .L3
                .L2:
                    sub rax, 0x1
                .L3:
                    mov rbx, rax
                )"
            },
            {
                /* no upward-exposed registers at all: everything is a local computation */
                "no_params_pure_constant",
                R"(
                    mov eax, 5
                    add eax, 3
                    ret
                )"
            },
            {
                /* two params, immediate-heavy arithmetic, exercises literal formatting */
                "immediate_heavy",
                R"(
                    mov eax, edi
                    shl eax, 2
                    and eax, 0xff
                    or eax, esi
                    ret
                )"
            },
            {
                /* AT&T syntax smoke test -- same shape as straightline_params */
                "att_straightline",
                R"(
                    movl %edi, %eax
                    addl %esi, %eax
                    retq
                )",
                false
            },
        };
    }
}

int main() {
    std::vector<TestCase> tests = BuildTestCases();
    std::cout << "Running " << tests.size() << " test cases \n";

    for (const auto& tc : tests) {
        RunTestCase(tc);
    }
    return 0;
}
