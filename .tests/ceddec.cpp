#include <ceddec/types.hpp>
#include <ceddec/calling_conv.hpp>
#include <ceddec/formatter.hpp>
#include <ceddec/printer.hpp>

#include <ceddec/parser.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/ast.hpp>
#include <ceddec/emitters/c.hpp>

#include <iostream>
#include <cassert>

void DoDecomp(std::string& asm_code, bool intel = true) {
    try {
        auto parser_inst = ceddec::CreateParser({
            .arch = ceddec::Architecture::x86_64,
            .intel_syntax = true,
            .strict_mode = true,
            .default_hex_immediates = false
        });
        std::cerr << "[debug] intel param = " << intel << "\n";

        ceddec::ParsedBlock parsed_block = parser_inst->ParseBlock(asm_code);
        assert(!parsed_block.instructions.empty());

        ceddec::ControlFlowGraph cfg_inst = ceddec::IRLifter::BuildCFG(parsed_block.instructions);

        ceddec::IRLifter::CalculateDominators(cfg_inst);
        ceddec::IRLifter::CalculateDominanceFrontiers(cfg_inst);
        ceddec::IRLifter::InsertPhiNodes(cfg_inst);
        ceddec::IRLifter::RenameVariables(cfg_inst);

        std::shared_ptr<ceddec::ASTBlockStmt> ast_tree = ceddec::ASTBuilder::BuildAST(cfg_inst);
        assert(ast_tree != nullptr);

        ceddec::FunctionSignature fn_signature = ceddec::AnalyzeCallingConvention(cfg_inst);
        std::string fn_header = ceddec::FormatFunctionHeader(fn_signature);

        ceddec::CEmitter c_emitter_inst;
        std::string emitted_code = c_emitter_inst.EmitFunction(fn_header, fn_signature.return_type, *ast_tree);

        std::stringstream ss(emitted_code);
        std::string line;

        while (std::getline(ss, line)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                continue;
            }
            size_t last = line.find_last_not_of(" \t\r\n");

            std::string trimmed = line.substr(first, (last - first + 1));
            std::cout << ">> " << trimmed << "\n";
        }

    } catch (const std::exception& err_obj) {
        std::cerr << "Caught exception: " << err_obj.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
    }
}

int main() {
    std::cout << "ceddec CLI v0.0.1\n"
              << "Copyright (c) 2026 ceddec. All rights reserved.\n"
              << "Type 'run(intel)'| 'run(att)' to decompile, 'clear()' to reset, or 'quit()' to exit.\n";

    std::string line;
    std::string asm_buffer;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "run()" || line == "run(intel)" || line == "run(att)") {
            if (!asm_buffer.empty()) {
                bool intel = (line != "run(att)");
                DoDecomp(asm_buffer, intel);
                asm_buffer.clear();
            } else {
                std::cout << "[!] Buffer is empty.\n";
            }
        } else if (line == "clear()") {
            asm_buffer.clear();
            std::cout << "[+] Assembly buffer cleared.\n";
        } else if (line == "quit()") {
            break;
        } else {
            asm_buffer += line + "\n";
        }
    }

    return 0;
}
