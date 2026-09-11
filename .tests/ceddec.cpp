#include <ceddec/types.hpp>
#include <ceddec/calling_conv.hpp>
#include <ceddec/formatter.hpp>
#include <ceddec/printer.hpp>
#include <ceddec/naming.hpp>

#include <ceddec/parser.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/ast.hpp>
#include <ceddec/emitters/c.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>

namespace {
    struct Session {
        bool verbose = false;   /* dump parsed instructions + block count before emitting C */
    };

    Session g_session;

    void PrintHelp() {
        std::cout <<
            "Commands:\n"
            "  run(intel)        decompile buffer as Intel syntax\n"
            "  run(att)          decompile buffer as AT&T syntax\n"
            "  run()             same as run(intel)\n"
            "  load <path>       read a file's contents into the buffer (replaces it)\n"
            "  show()            print the current buffer contents\n"
            "  clear()           reset the buffer\n"
            "  verbose(on|off)   toggle dumping parsed instructions before emitting C\n"
            "  naming(default)   IDA-style naming: a1, a2, ... (this is also the default)\n"
            "  naming(arg0)      arg0, arg1, ... naming\n"
            "  naming(short)     terse p0, p1, ... naming\n"
            "  help              show this message\n"
            "  quit()            exit\n";
    }

    /* prints one line per parsed instruction */
    void DumpParsedBlock(const ceddec::ParsedBlock& block) {
        std::cout << "[debug] " << block.instructions.size() << " instruction(s) parsed:\n";
        for (const auto& inst : block.instructions) {
            std::cout << "  " << (inst.is_valid ? "  " : "? ") << inst.mnemonic;
            for (size_t i = 0; i < inst.operands.size(); ++i) {
                std::cout << (i == 0 ? " " : ", ") << inst.operands[i].raw_text;
            }
            std::cout << "\n";
        }
    }

    void DumpCFG(const ceddec::ControlFlowGraph& cfg) {
        std::cout << "[debug] CFG: " << cfg.blocks.size() << " block(s)\n";
        for (const auto& b : cfg.blocks) {
            std::cout << "  block " << b.id << ": " << b.instructions.size() << " ir instr, successors = {";
            for (size_t i = 0; i < b.successors.size(); ++i) {
                std::cout << (i ? ", " : "") << b.successors[i];
            }
            std::cout << "}\n";
        }
    }

    /* applies the current naming preset; called from the naming(...) command */
    void ApplyNamingPreset(const std::string& preset) {
        if (preset == "default" || preset == "ida") {
            ceddec::ActiveNamingScheme() = ceddec::NamingScheme{};
            std::cout << "[+] Naming scheme reset to default\n";
        } else if (preset == "arg0") {
            ceddec::ActiveNamingScheme().param_name = [](size_t i) {
                return "arg" + std::to_string(i);
            };
            std::cout << "[+] Naming scheme set to arg0, arg1, ...\n";
        } else if (preset == "short") {
            ceddec::ActiveNamingScheme().param_name = [](size_t i) {
                return "p" + std::to_string(i);
            };
            std::cout << "[+] Naming scheme set to short (p0, p1, ...).\n";
        } else {
            std::cout << "[!] Unknown naming preset \"" << preset << "\". Try naming(default), naming(arg0), or naming(short).\n";
        }
    }

    bool LoadFileIntoBuffer(const std::string& path, std::string& out_buffer) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::cout << "[!] Could not open file: " << path << "\n";
            return false;
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        out_buffer = contents.str();
        if (out_buffer.empty()) {
            std::cout << "[!] File is empty: " << path << "\n";
            return false;
        }
        return true;
    }

}

void DoDecomp(std::string& asm_code, bool intel) {
    try {
        auto parser_inst = ceddec::CreateParser({
            .arch = ceddec::Architecture::x86_64,
            .intel_syntax = intel,
            .strict_mode = true,
            .default_hex_immediates = false
        });

        ceddec::ParsedBlock parsed_block = parser_inst->ParseBlock(asm_code);

        if (parsed_block.instructions.empty()) {
            std::cout << "[!] Nothing valid to decompile: no instructions were parsed from the buffer.\n";
            return;
        }

        if (g_session.verbose) {
            DumpParsedBlock(parsed_block);
        }

        ceddec::ControlFlowGraph cfg_inst = ceddec::IRLifter::BuildCFG(parsed_block.instructions);

        ceddec::IRLifter::CalculateDominators(cfg_inst);
        ceddec::IRLifter::CalculateDominanceFrontiers(cfg_inst);
        ceddec::IRLifter::InsertPhiNodes(cfg_inst);
        ceddec::IRLifter::RenameVariables(cfg_inst);

        if (g_session.verbose) {
            DumpCFG(cfg_inst);
        }

        std::shared_ptr<ceddec::ASTBlockStmt> ast_tree = ceddec::ASTBuilder::BuildAST(cfg_inst);
        if (!ast_tree) {
            std::cout << "[!] Internal error: AST construction returned null.\n";
            return;
        }

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
              << "Type 'help' for a list of commands.\n";

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
        } else if (line == "show()") {
            if (asm_buffer.empty()) {
                std::cout << "[!] Buffer is empty.\n";
            } else {
                std::cout << asm_buffer;
            }
        } else if (line == "verbose(on)") {
            g_session.verbose = true;
            std::cout << "[+] Verbose mode on.\n";
        } else if (line == "verbose(off)") {
            g_session.verbose = false;
            std::cout << "[+] Verbose mode off.\n";
        } else if (line == "naming(default)") {
            ApplyNamingPreset("default");
        } else if (line == "naming(short)") {
            ApplyNamingPreset("short");
        } else if (line.starts_with("load ")) {
            std::string path = line.substr(5);
            std::string loaded;
            if (LoadFileIntoBuffer(path, loaded)) {
                asm_buffer = loaded;
                std::cout << "[+] Loaded " << asm_buffer.size() << " byte(s) from " << path << " into buffer.\n";
            }
        } else if (line == "help") {
            PrintHelp();
        } else if (line == "quit()") {
            break;
        } else if (line.empty()) {
            /* ignore blank lines rather than stuffing them into the asm buffer */
            continue;
        } else {
            asm_buffer += line + "\n";
        }
    }

    return 0;
}
