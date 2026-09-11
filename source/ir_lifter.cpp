#include <ceddec/types.hpp>
#include <ceddec/ir.hpp>

#include <source/misc/opcode.hpp>
#include <source/misc/register.hpp>

#include <tools.hpp>
#include <functional>

namespace {
    using namespace ceddec;

    /* safely extract branch/jump target label from ParsedOperand */
    static std::string ExtractTargetString(const ParsedOperand& op) {
        if (op.type == OperandType::Label || op.type == OperandType::Register || op.type == OperandType::Memory) {
            return std::string(op.raw_text);
        }
        return "";
    }

    static void PopulateIROperand(const ceddec::ParsedOperand& src_op, ceddec::IROperand& out_ir_op) {
        if (src_op.type == ceddec::OperandType::Register) {
            out_ir_op.value = ParseRegister(src_op.raw_text);
        } else if (src_op.type == ceddec::OperandType::Memory) {
            out_ir_op.value = src_op.mem;
        } else if (src_op.type == ceddec::OperandType::Immediate) {
            out_ir_op.value = src_op.immediate_val;
        } else if (src_op.type == ceddec::OperandType::Label) {
            out_ir_op.value = std::string(src_op.raw_text);
        }
    }

    static std::vector<ceddec::IRInstruction> LiftInstructionImpl(const ceddec::ParsedInstruction& parsed_inst, size_t inst_idx) {
        std::vector<ceddec::IRInstruction> lifted_ops;
        if (!parsed_inst.is_valid) return lifted_ops;

        std::string m_lower = std::string(parsed_inst.mnemonic);
        std::transform(m_lower.begin(), m_lower.end(), m_lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        auto is_frame_reg = [](const ceddec::ParsedOperand& op) {
            if (op.type != ceddec::OperandType::Register) return false;
            std::string reg_name = ToLower(op.raw_text);
            return reg_name == "rbp" || reg_name == "ebp" || reg_name == "rsp" || reg_name == "esp";
        };

        /* skip function prologue/epilogue stack management */
        if (m_lower == "push" || m_lower == "pop") {
            if (!parsed_inst.operands.empty() && is_frame_reg(parsed_inst.operands[0])) {
                return lifted_ops; /* Filter push rbp / pop rbp */
            }
        }

        if (m_lower == "leave") {
            return lifted_ops; /* filter frame epilogue */
        }

        /* skip frame pointer assignments (e.g. mov rbp, rsp or mov rsp, rbp) */
        if (m_lower == "mov" && parsed_inst.operands.size() >= 2) {
            if (is_frame_reg(parsed_inst.operands[0]) && is_frame_reg(parsed_inst.operands[1])) {
                return lifted_ops;
            }
        }

        /* skip stack allocation / deallocation (e.g. sub rsp, 16 or add rsp, 16) */
        if ((m_lower == "sub" || m_lower == "add") && !parsed_inst.operands.empty()) {
            if (is_frame_reg(parsed_inst.operands[0])) {
                return lifted_ops;
            }
        }

        /* handle standard non-frame stack pushes/pops */
        if (m_lower == "push") {
            ceddec::IRInstruction store;
            store.opcode = ceddec::IROpcode::Assign;
            store.destination.value = ceddec::MemoryOperand{"rsp", "", 1, 0};
            if (!parsed_inst.operands.empty()) PopulateIROperand(parsed_inst.operands[0], store.source);
            store.original_index = inst_idx;
            store.is_valid = true;
            lifted_ops.push_back(store);
            return lifted_ops;
        }

        if (m_lower == "pop") {
            ceddec::IRInstruction load;
            load.opcode = ceddec::IROpcode::Assign;
            if (!parsed_inst.operands.empty()) PopulateIROperand(parsed_inst.operands[0], load.destination);
            load.source.value = ceddec::MemoryOperand{"rsp", "", 1, 0};
            load.original_index = inst_idx;
            load.is_valid = true;
            lifted_ops.push_back(load);
            return lifted_ops;
        }

        /*
         * calls are lifted explicitly rather than through the generic fallback
         * below: the call target (a Label, or occasionally a Register/Memory for
         * an indirect call) must always end up as ir.destination, guaranteed
         * non-empty whenever the parser produced any operand at all. Going
         * through the generic path relies on operands[0] always being exactly
         * the callee with nothing else going on, and silently produces a
         * default (Register::Unknown) destination if that assumption doesn't
         * hold for some parser path, which then prints as a bogus "rax" in
         * emitted C, indistinguishable from an actual empty operand.
         */
        if (m_lower == "call") {
            ceddec::IRInstruction call_ir;
            call_ir.opcode = ceddec::IROpcode::Call;
            call_ir.condition = parsed_inst.condition;
            call_ir.original_index = inst_idx;
            if (!parsed_inst.operands.empty()) {
                const auto& target_op = parsed_inst.operands[0];
                if (target_op.type == ceddec::OperandType::Register) {
                    /*
                     * a bare call target symbol (e.g. "call helper") can get
                     * misclassified upstream as a Register operand if the
                     * parser's heuristic for "unadorned word" defaults to
                     * assuming a register. If it doesn't actually resolve to a
                     * real register, it's a callee symbol, not garbage,
                     * fall back to keeping the raw text rather than silently
                     * collapsing to Register::Unknown (which prints as a bogus
                     * "rax" downstream, indistinguishable from a truly missing
                     * operand).
                     */
                    ceddec::Register resolved = ParseRegister(target_op.raw_text);
                    if (resolved != ceddec::Register::Unknown) {
                        call_ir.destination.value = resolved;
                    } else {
                        call_ir.destination.value = std::string(target_op.raw_text);
                    }
                } else if (target_op.type == ceddec::OperandType::Memory) {
                    call_ir.destination.value = target_op.mem;
                } else {
                    /* Label, or anything else the parser produced: keep the raw text verbatim */
                    call_ir.destination.value = std::string(target_op.raw_text);
                }
            }
            call_ir.is_valid = true;
            lifted_ops.push_back(call_ir);
            return lifted_ops;
        }

        /* standard 1-to-1 fallback */
        ceddec::IRInstruction ir;
        ir.opcode = MapOpcode(m_lower);
        ir.condition = parsed_inst.condition;
        ir.original_index = inst_idx;

        if (parsed_inst.operands.size() >= 1) PopulateIROperand(parsed_inst.operands[0], ir.destination);
        if (parsed_inst.operands.size() >= 2) PopulateIROperand(parsed_inst.operands[1], ir.source);

        ir.is_valid = true;
        lifted_ops.push_back(ir);

        /* implicit flag updates */
        bool touches_flags = (
            ir.opcode == ceddec::IROpcode::Compare || ir.opcode == ceddec::IROpcode::Test ||
            ir.opcode == ceddec::IROpcode::Add     || ir.opcode == ceddec::IROpcode::Sub ||
            ir.opcode == ceddec::IROpcode::AddCarry || ir.opcode == ceddec::IROpcode::SubBorrow ||
            ir.opcode == ceddec::IROpcode::And     || ir.opcode == ceddec::IROpcode::Or  || ir.opcode == ceddec::IROpcode::Xor ||
            ir.opcode == ceddec::IROpcode::Shl     || ir.opcode == ceddec::IROpcode::Shr ||
            ir.opcode == ceddec::IROpcode::Rol     || ir.opcode == ceddec::IROpcode::Ror ||
            ir.opcode == ceddec::IROpcode::Inc     || ir.opcode == ceddec::IROpcode::Dec || ir.opcode == ceddec::IROpcode::Neg ||
            ir.opcode == ceddec::IROpcode::Mul     || ir.opcode == ceddec::IROpcode::Div ||
            ir.opcode == ceddec::IROpcode::BitTest || ir.opcode == ceddec::IROpcode::BitSet ||
            ir.opcode == ceddec::IROpcode::BitReset || ir.opcode == ceddec::IROpcode::BitComplement
        );

        if (touches_flags) {
            ceddec::IRInstruction flag_update;
            flag_update.opcode = ceddec::IROpcode::Assign;
            flag_update.destination.value = ceddec::Register::RFLAGS;
            flag_update.original_index = inst_idx;
            flag_update.is_valid = true;
            lifted_ops.push_back(flag_update);
        }

        return lifted_ops;
    }

    /* true if `op`'s opcode reads its destination in addition to writing it
       (arithmetic/logic RMW ops, and pure-read ops like compare/test) */
    static bool DestIsAlsoRead(ceddec::IROpcode op) {
        using ceddec::IROpcode;
        switch (op) {
            case IROpcode::Add: case IROpcode::Sub:
            case IROpcode::AddCarry: case IROpcode::SubBorrow:
            case IROpcode::And: case IROpcode::Or: case IROpcode::Xor:
            case IROpcode::Shl: case IROpcode::Shr: case IROpcode::Rol: case IROpcode::Ror:
            case IROpcode::Inc: case IROpcode::Dec: case IROpcode::Neg: case IROpcode::Not:
            case IROpcode::Compare: case IROpcode::Test:
            case IROpcode::BitTest: case IROpcode::BitSet:
            case IROpcode::BitReset: case IROpcode::BitComplement:
            case IROpcode::Mul: case IROpcode::Div:
                return true;
            default:
                return false;
        }
    }

    /* opcodes whose "destination" is purely read, never written (comparisons) */
    static bool IsPureReadDest(ceddec::IROpcode op) {
        using ceddec::IROpcode;
        return op == IROpcode::Compare || op == IROpcode::Test || op == IROpcode::BitTest;
    }

    static void RenameBlockDFS(size_t block_id, ceddec::ControlFlowGraph& cfg, ceddec::SSARenameState& state) {
        auto& block = cfg.blocks[block_id];
        std::vector<ceddec::Register> pushed_regs;

        /* process Phi destinations first */
        for (auto& inst : block.instructions) {
            if (inst.opcode == ceddec::IROpcode::Phi) {
                if (std::holds_alternative<ceddec::Register>(inst.destination.value)) {
                    ceddec::Register reg = std::get<ceddec::Register>(inst.destination.value);
                    if (reg != ceddec::Register::Unknown) {
                        uint32_t ver = ++state.counters[reg];
                        inst.destination.ssa_version = ver;
                        state.stacks[reg].push_back(ver);
                        pushed_regs.push_back(reg);
                    }
                }
            }
        }

        /* process non-Phi instructions */
        for (auto& inst : block.instructions) {
            if (inst.opcode == ceddec::IROpcode::Phi) continue;

            bool is_read_only_dest = (inst.opcode == ceddec::IROpcode::Compare || inst.opcode == ceddec::IROpcode::Test || inst.opcode == ceddec::IROpcode::BitTest);

            /* source operands read active stack top */
            if (std::holds_alternative<ceddec::Register>(inst.source.value)) {
                ceddec::Register reg = std::get<ceddec::Register>(inst.source.value);
                if (reg != ceddec::Register::Unknown && !state.stacks[reg].empty()) {
                    inst.source.ssa_version = state.stacks[reg].back();
                }
            }

            if (is_read_only_dest) {
                /* destination operand for CMP/TEST/BITTEST is ALSO a READ, not a write */
                if (std::holds_alternative<ceddec::Register>(inst.destination.value)) {
                    ceddec::Register reg = std::get<ceddec::Register>(inst.destination.value);
                    if (reg != ceddec::Register::Unknown && !state.stacks[reg].empty()) {
                        inst.destination.ssa_version = state.stacks[reg].back();
                    }
                }
            } else {
                /* destination operands push a new version */
                if (std::holds_alternative<ceddec::Register>(inst.destination.value)) {
                    ceddec::Register reg = std::get<ceddec::Register>(inst.destination.value);
                    if (reg != ceddec::Register::Unknown) {
                        inst.destination.pre_write_version = state.stacks[reg].empty()
                            ? 0
                            : state.stacks[reg].back();
                        uint32_t ver = ++state.counters[reg];
                        inst.destination.ssa_version = ver;
                        state.stacks[reg].push_back(ver);
                        pushed_regs.push_back(reg);
                    }
                }
            }
        }

        /* fill in incoming values for successor Phi nodes */
        for (size_t succ_id : block.successors) {
            auto& succ = cfg.blocks[succ_id];

            size_t pred_index = 0;
            for (size_t p = 0; p < succ.predecessors.size(); ++p) {
                if (succ.predecessors[p] == block_id) {
                    pred_index = p;
                    break;
                }
            }

            for (auto& inst : succ.instructions) {
                if (inst.opcode == ceddec::IROpcode::Phi) {
                    if (std::holds_alternative<ceddec::Register>(inst.destination.value)) {
                        ceddec::Register reg = std::get<ceddec::Register>(inst.destination.value);
                        if (reg != ceddec::Register::Unknown && pred_index < inst.phi_sources.size()) {
                            if (!state.stacks[reg].empty()) {
                                inst.phi_sources[pred_index].ssa_version = state.stacks[reg].back();
                            }
                        }
                    }
                }
            }
        }

        /* traverse dominator tree children */
        for (size_t child_id = 0; child_id < cfg.blocks.size(); ++child_id) {
            if (cfg.blocks[child_id].idom == block_id && child_id != block_id) {
                RenameBlockDFS(child_id, cfg, state);
            }
        }

        /* pop stack entries pushed by this block scope */
        for (const auto& reg : pushed_regs) {
            state.stacks[reg].pop_back();
        }
    }
}

namespace ceddec {
    std::vector<IRInstruction> IRLifter::LiftInstruction(const ParsedInstruction& parsed_inst) {
        return LiftInstructionImpl(parsed_inst, 0);
    }

    ControlFlowGraph IRLifter::BuildCFG(const std::vector<ParsedInstruction>& instructions) {
        ControlFlowGraph cfg;
        if (instructions.empty()) return cfg;

        std::map<std::string, size_t> label_to_inst_idx;
        std::set<size_t> leaders = {0};

        /* pass 1: record all explicit label definition indices */
        for (size_t i = 0; i < instructions.size(); ++i) {
            const auto& inst = instructions[i];
            if (inst.mnemonic == "label" && !inst.operands.empty()) {
                label_to_inst_idx[std::string(inst.operands[0].raw_text)] = i;
            }
        }

        /* pass 2: identify basic block leaders */
        for (size_t i = 0; i < instructions.size(); ++i) {
            const auto& inst = instructions[i];

            if (inst.mnemonic == "jmp" || (inst.mnemonic.starts_with("j") && inst.condition != ConditionCode::None)) {
                if (!inst.operands.empty()) {
                    std::string target = ExtractTargetString(inst.operands[0]);
                    if (label_to_inst_idx.count(target)) {
                        leaders.insert(label_to_inst_idx[target]);
                    }
                }
                if (i + 1 < instructions.size()) {
                    leaders.insert(i + 1);
                }
            } else if (inst.mnemonic == "ret") {
                if (i + 1 < instructions.size()) {
                    leaders.insert(i + 1);
                }
            }
        }

        /* pass 3: populate basic blocks and lift instructions */
        std::map<size_t, size_t> inst_to_block_id;
        BasicBlock current_bb;
        size_t current_id = 0;

        for (size_t i = 0; i < instructions.size(); ++i) {
            if (leaders.count(i) && !current_bb.instructions.empty()) {
                current_bb.id = current_id++;
                cfg.blocks.push_back(current_bb);
                current_bb = BasicBlock();
            }

            inst_to_block_id[i] = current_id;

            /* lift single ParsedInstruction into IR instructions */
            std::vector<IRInstruction> lifted = LiftInstructionImpl(instructions[i], i);
            for (auto& ir_inst : lifted) {
                current_bb.instructions.push_back(ir_inst);
            }
        }

        if (!current_bb.instructions.empty()) {
            current_bb.id = current_id;
            cfg.blocks.push_back(current_bb);
        }

        /* pass 4: connect control flow edges and compute predecessors */
        for (size_t b = 0; b < cfg.blocks.size(); ++b) {
            auto& bb = cfg.blocks[b];

            /* find the last parsed instruction corresponding to this block */
            size_t last_inst_idx = 0;
            for (auto it = inst_to_block_id.rbegin(); it != inst_to_block_id.rend(); ++it) {
                if (it->second == b) {
                    last_inst_idx = it->first;
                    break;
                }
            }

            const auto& last_parsed = instructions[last_inst_idx];

            if (last_parsed.mnemonic == "jmp") {
                if (!last_parsed.operands.empty()) {
                    std::string target = ExtractTargetString(last_parsed.operands[0]);
                    if (label_to_inst_idx.count(target)) {
                        size_t target_b = inst_to_block_id[label_to_inst_idx[target]];
                        bb.successors.push_back(target_b);
                    }
                }
            } else if (last_parsed.mnemonic.starts_with("j") && last_parsed.condition != ConditionCode::None) {
                if (!last_parsed.operands.empty()) {
                    std::string target = ExtractTargetString(last_parsed.operands[0]);
                    if (label_to_inst_idx.count(target)) {
                        size_t target_b = inst_to_block_id[label_to_inst_idx[target]];
                        bb.successors.push_back(target_b);
                    }
                }
                if (b + 1 < cfg.blocks.size()) {
                    bb.successors.push_back(b + 1);
                }
            } else if (last_parsed.mnemonic != "ret") {
                if (b + 1 < cfg.blocks.size()) {
                    bb.successors.push_back(b + 1);
                }
            }
        }

        /* update predecessors based on successors */
        for (size_t b = 0; b < cfg.blocks.size(); ++b) {
            for (size_t succ : cfg.blocks[b].successors) {
                cfg.blocks[succ].predecessors.push_back(b);
            }
        }

        return cfg;
    }

    void IRLifter::ApplyLocalSSA(ControlFlowGraph& cfg) {
        /* tracks the current version of each physical register */
        uint32_t next_version = 1;

        for (auto& block : cfg.blocks) {
            std::unordered_map<Register, uint32_t> current_versions;

            for (auto& inst : block.instructions) {
                bool is_read_only_dest = (inst.opcode == IROpcode::Compare || inst.opcode == IROpcode::Test || inst.opcode == IROpcode::BitTest);

                /* update source to the current version */
                if (std::holds_alternative<Register>(inst.source.value)) {
                    Register src_reg = std::get<Register>(inst.source.value);
                    if (current_versions.find(src_reg) != current_versions.end()) {
                        inst.source.ssa_version = current_versions[src_reg];
                    }
                }

                if (is_read_only_dest) {
                    if (std::holds_alternative<Register>(inst.destination.value)) {
                        Register dst_reg = std::get<Register>(inst.destination.value);
                        if (current_versions.find(dst_reg) != current_versions.end()) {
                            inst.destination.ssa_version = current_versions[dst_reg];
                        }
                    }
                } else {
                    /* bump the version for the destination */
                    if (std::holds_alternative<Register>(inst.destination.value)) {
                        Register dst_reg = std::get<Register>(inst.destination.value);
                        uint32_t new_version = next_version++;
                        current_versions[dst_reg] = new_version;
                        inst.destination.ssa_version = new_version;
                    }
                }
            }
        }
    }

    void IRLifter::CalculateDominators(ControlFlowGraph& cfg) {
        if (cfg.blocks.empty()) return;

        std::vector<size_t> all_nodes;
        for (size_t i = 0; i < cfg.blocks.size(); ++i) {
            all_nodes.push_back(i);
        }

        /* entry block dominates only itself, others are dominated by all */
        cfg.blocks[0].dominators = {0};
        for (size_t i = 1; i < cfg.blocks.size(); ++i) {
            cfg.blocks[i].dominators = all_nodes;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 1; i < cfg.blocks.size(); ++i) {
                if (cfg.blocks[i].predecessors.empty()) continue;

                /* intersection of all preceding dominators */
                std::vector<size_t> new_doms = cfg.blocks[cfg.blocks[i].predecessors[0]].dominators;
                for (size_t p = 1; p < cfg.blocks[i].predecessors.size(); ++p) {
                    size_t pred = cfg.blocks[i].predecessors[p];
                    std::vector<size_t> intersection;
                    std::set_intersection(
                        new_doms.begin(), new_doms.end(),
                        cfg.blocks[pred].dominators.begin(), cfg.blocks[pred].dominators.end(),
                        std::back_inserter(intersection)
                    );
                    new_doms = intersection;
                }

                /* a block always dominates itself */
                new_doms.push_back(i);
                std::sort(new_doms.begin(), new_doms.end());
                new_doms.erase(std::unique(new_doms.begin(), new_doms.end()), new_doms.end());

                if (new_doms != cfg.blocks[i].dominators) {
                    cfg.blocks[i].dominators = new_doms;
                    changed = true;
                }
            }
        }

        /* find immediate dominator (idom), the closest dominator to the block */
        for (size_t i = 1; i < cfg.blocks.size(); ++i) {
            for (size_t dom : cfg.blocks[i].dominators) {
                if (dom == i) continue;
                bool is_immediate = true;
                for (size_t other_dom : cfg.blocks[i].dominators) {
                    if (other_dom != i && other_dom != dom) {
                        auto& other_doms = cfg.blocks[other_dom].dominators;
                        if (std::find(other_doms.begin(), other_doms.end(), dom) != other_doms.end()) {
                            is_immediate = false;
                            break;
                        }
                    }
                }
                if (is_immediate) {
                    cfg.blocks[i].idom = dom;
                    break;
                }
            }
        }
    }

    void IRLifter::CalculateDominanceFrontiers(ControlFlowGraph& cfg) {
        for (auto& block : cfg.blocks) {
            block.dom_frontier.clear();
        }

        for (size_t i = 0; i < cfg.blocks.size(); ++i) {
            if (cfg.blocks[i].predecessors.size() >= 2) {
                for (size_t pred : cfg.blocks[i].predecessors) {
                    size_t runner = pred;
                    while (runner != cfg.blocks[i].idom && runner != static_cast<size_t>(-1)) {
                        auto& frontier = cfg.blocks[runner].dom_frontier;
                        if (std::find(frontier.begin(), frontier.end(), i) == frontier.end()) {
                            frontier.push_back(i);
                        }
                        runner = cfg.blocks[runner].idom;
                    }
                }
            }
        }
    }

    void IRLifter::InsertPhiNodes(ControlFlowGraph& cfg) {
        /* map each register to the set of block IDs where it is written to */
        std::unordered_map<Register, std::set<size_t>> def_sites;

        for (const auto& block : cfg.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<Register>(inst.destination.value)) {
                    Register reg = std::get<Register>(inst.destination.value);
                    if (reg != Register::Unknown) {
                        def_sites[reg].insert(block.id);
                    }
                }
            }
        }

        /* place Phi-nodes at dominance frontiers */
        for (const auto& [reg, blocks] : def_sites) {
            std::vector<size_t> worklist(blocks.begin(), blocks.end());
            std::set<size_t> inserted_has_phi;

            while (!worklist.empty()) {
                size_t current_block_id = worklist.back();
                worklist.pop_back();

                for (size_t frontier_id : cfg.blocks[current_block_id].dom_frontier) {
                    if (inserted_has_phi.find(frontier_id) == inserted_has_phi.end()) {
                        inserted_has_phi.insert(frontier_id);

                        /* construct the Phi instruction */
                        IRInstruction phi_inst;
                        phi_inst.opcode = IROpcode::Phi;
                        phi_inst.destination.value = reg;

                        /* create a dummy source slot for each predecessor block */
                        size_t num_preds = cfg.blocks[frontier_id].predecessors.size();
                        for (size_t p = 0; p < num_preds; ++p) {
                            IROperand src_op;
                            src_op.value = reg;
                            phi_inst.phi_sources.push_back(src_op);
                        }

                        /* insert at the very top of the basic block */
                        cfg.blocks[frontier_id].instructions.insert(
                            cfg.blocks[frontier_id].instructions.begin(),
                            phi_inst
                        );

                        /* propagate if this frontier block also creates a new definition point */
                        if (blocks.find(frontier_id) == blocks.end()) {
                            worklist.push_back(frontier_id);
                        }
                    }
                }
            }
        }
    }

    void IRLifter::RenameVariables(ControlFlowGraph& cfg) {
        if (cfg.blocks.empty()) return;
        SSARenameState state;
        RenameBlockDFS(0, cfg, state);
    }

    std::set<Register> IRLifter::FindUpwardExposedRegisters(const ControlFlowGraph& cfg) {
        std::set<Register> live_in;
        if (cfg.blocks.empty()) return live_in;

        std::set<size_t> visited;

        std::function<void(size_t, std::set<Register>)> walk =
            [&](size_t block_id, std::set<Register> written_so_far) {
            if (block_id >= cfg.blocks.size() || !visited.insert(block_id).second) return;
            const auto& block = cfg.blocks[block_id];

            auto check_read = [&](const IROperand& op) {
                if (std::holds_alternative<Register>(op.value)) {
                    Register r = std::get<Register>(op.value);
                    if (r != Register::Unknown && !written_so_far.count(r)) {
                        live_in.insert(r);
                    }
                }
            };
            auto check_write = [&](const IROperand& op) {
                if (std::holds_alternative<Register>(op.value)) {
                    Register r = std::get<Register>(op.value);
                    if (r != Register::Unknown) written_so_far.insert(r);
                }
            };

            for (const auto& inst : block.instructions) {
                if (inst.opcode == IROpcode::Phi) continue;

                check_read(inst.source);
                if (DestIsAlsoRead(inst.opcode)) {
                    check_read(inst.destination);
                }
                if (!IsPureReadDest(inst.opcode)) {
                    check_write(inst.destination);
                }
            }

            for (size_t succ : block.successors) {
                walk(succ, written_so_far);
            }
        };

        walk(0, {});
        return live_in;
    }

    Register IRLifter::RegisterFamily64(Register r) {
        switch (r) {
            case Register::RDI: case Register::EDI: case Register::DI: case Register::DIL:
                return Register::RDI;
            case Register::RSI: case Register::ESI: case Register::SI: case Register::SIL:
                return Register::RSI;
            case Register::RDX: case Register::EDX: case Register::DX:
            case Register::DL: case Register::DH:
                return Register::RDX;
            case Register::RCX: case Register::ECX: case Register::CX:
            case Register::CL: case Register::CH:
                return Register::RCX;
            case Register::R8: case Register::R8D: case Register::R8W: case Register::R8B:
                return Register::R8;
            case Register::R9: case Register::R9D: case Register::R9W: case Register::R9B:
                return Register::R9;
            default:
                return r;
        }
    }

    const std::vector<Register>& IRLifter::SysVIntArgOrder() {
        static const std::vector<Register> order = {
            Register::RDI, Register::RSI, Register::RDX, Register::RCX, Register::R8, Register::R9
        };
        return order;
    }

    std::map<Register, size_t> IRLifter::BuildParamIndexMap(const std::set<Register>& live_in) {
        std::map<Register, size_t> result;
        std::set<Register> families;
        for (Register r : live_in) {
            families.insert(RegisterFamily64(r));
        }

        size_t idx = 0;
        for (Register candidate : SysVIntArgOrder()) {
            if (families.count(candidate)) {
                result[candidate] = idx++;
            }
        }
        return result;
    }

    bool IRLifter::IsBackEdge(const ControlFlowGraph& cfg, size_t from_block, size_t to_block) {
        if (to_block >= cfg.blocks.size()) return false;
        const auto& to_doms = cfg.blocks[to_block].dominators;
        return std::find(to_doms.begin(), to_doms.end(), to_block) != to_doms.end()
            && std::find(cfg.blocks[from_block].dominators.begin(), cfg.blocks[from_block].dominators.end(), to_block)
               != cfg.blocks[from_block].dominators.end();
    }
}
