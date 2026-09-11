#include <ceddec/types.hpp>
#include <ceddec/ast.hpp>
#include <ceddec/ir.hpp>
#include <ceddec/naming.hpp>
#include <source/misc/register_name.hpp>
#include <array>

namespace ceddec {
    static std::string ConditionCodeToString(ConditionCode cc) {
        switch (cc) {
            case ConditionCode::E:   return "==";
            case ConditionCode::NE:  return "!=";
            case ConditionCode::G:
            case ConditionCode::A:   return ">";
            case ConditionCode::GE:
            case ConditionCode::AE:  return ">=";
            case ConditionCode::L:
            case ConditionCode::B:   return "<";
            case ConditionCode::LE:
            case ConditionCode::BE:  return "<=";
            default:                 return "==";
        }
    }

    static std::string NegateConditionString(const std::string& op) {
        if (op == "==") return "!=";
        if (op == "!=") return "==";
        if (op == ">")  return "<=";
        if (op == ">=") return "<";
        if (op == "<")  return ">=";
        if (op == "<=") return ">";
        return op;
    }

    /*
     * appends "target = source_version;" copy assignments to the end of a
     * branch's statement list for every Phi node at `join_id` whose incoming
     * value from `from_block_id` needs to be made visible under the phi's
     * merged name. This is the standard "phi -> copy on each incoming edge"
     * lowering: InsertPhiNodes (ir_lifter.cpp) already records, per phi, one
     * phi_sources entry per predecessor of the join block; this function
     * consumes that data instead of discarding it (which is what
     * ConvertBlockToAST does by design, since Phi instructions aren't real
     * operations, they only exist to carry this merge information).
     *
     * Without this, code after a join block references the phi's destination
     * SSA version (e.g. "rax_v3") which nothing ever assigns.
     */
    static void AppendPhiCopiesForEdge(
        std::shared_ptr<ASTBlockStmt>& branch_ast,
        const ControlFlowGraph& cfg,
        size_t join_id,
        size_t from_block_id)
    {
        if (join_id >= cfg.blocks.size() || !branch_ast) return;
        const auto& join_block = cfg.blocks[join_id];

        size_t pred_index = 0;
        bool found_pred = false;
        for (size_t p = 0; p < join_block.predecessors.size(); ++p) {
            if (join_block.predecessors[p] == from_block_id) {
                pred_index = p;
                found_pred = true;
                break;
            }
        }
        if (!found_pred) return;

        for (const auto& inst : join_block.instructions) {
            if (inst.opcode != IROpcode::Phi) continue;
            if (!std::holds_alternative<Register>(inst.destination.value)) continue;
            if (pred_index >= inst.phi_sources.size()) continue;

            Register reg = std::get<Register>(inst.destination.value);
            if (reg == Register::Unknown) continue;

            std::string name = ActiveNamingScheme().local_name(reg, 0);
            uint32_t merged_ver = inst.destination.ssa_version;
            uint32_t incoming_ver = inst.phi_sources[pred_index].ssa_version;

            /* merged version must differ from the incoming one, or this is a no-op copy */
            if (merged_ver == 0 || merged_ver == incoming_ver) continue;

            auto target = std::make_shared<ASTVarExpr>(name, merged_ver);
            auto source = std::make_shared<ASTVarExpr>(name, incoming_ver);
            branch_ast->statements.push_back(std::make_shared<ASTAssignmentStmt>(target, source));
        }
    }

    /*
     * `param_index` maps a register (collapsed to its 64-bit family via IRLifter::RegisterFamily64)
     * to its 0-based position in calling-convention order. It is computed once in BuildAST via IRLifter::FindUpwardExposedRegisters
     * + IRLifter::BuildParamIndexMap, and threaded down through every function
     * below so the whole AST agrees on which registers are parameters.
     */
    using ParamIndexMap = std::map<Register, size_t>;

    static std::shared_ptr<ASTExpr> OperandToExpr(
        const IROperand& op,
        const ParamIndexMap& param_index,
        uint32_t override_version = 0)
    {
        uint32_t ver = override_version != 0 ? override_version : op.ssa_version;

        if (std::holds_alternative<Register>(op.value)) {
            Register reg = std::get<Register>(op.value);
            if (reg == Register::Unknown) {
                return std::make_shared<ASTVarExpr>(ActiveNamingScheme().local_name(Register::RAX, ver), ver);
            }

            /*
             * only the unversioned read of a parameter register (ssa_version == 0,
             * i.e. "still holds whatever the caller passed in, never reassigned")
             * prints as the parameter name. Once it's been written to, later
             * reads go through the ordinary versioned-local path below, since at
             * that point it no longer holds the incoming argument value.
             */
            if (ver == 0) {
                auto fit = param_index.find(IRLifter::RegisterFamily64(reg));
                if (fit != param_index.end()) {
                    return std::make_shared<ASTVarExpr>(ActiveNamingScheme().param_name(fit->second), 0);
                }
            }
            return std::make_shared<ASTVarExpr>(ActiveNamingScheme().local_name(reg, ver), ver);
        }
        else if (std::holds_alternative<int64_t>(op.value)) {
            return std::make_shared<ASTLiteralExpr>(static_cast<uint64_t>(std::get<int64_t>(op.value)));
        }
        else if (std::holds_alternative<MemoryOperand>(op.value)) {
            const auto& mem = std::get<MemoryOperand>(op.value);

            /*
             * every rbp/rsp-relative stack slot gets its own stable name, always
             * via stack_local_name, regardless of its offset. This used to special-
             * case offset==4 and offset==8 as "the" first and second parameter
             * slots, which collided with register-based parameter naming (a
             * parameter spilled to [rbp-4] and the register a1 could print as the
             * same name "a1", making a genuine store look like a no-op self-
             * assignment, and making later writes to the stack slot silently
             * overwrite what looked like the parameter itself). Stack slots and
             * register-based parameters are different values with independent
             * lifetimes and must never share a name.
             */
            if ((mem.base == "rbp" || mem.base == "ebp" || mem.base == "rsp" || mem.base == "esp") && mem.index.empty()) {
                int64_t abs_offset = std::abs(mem.displacement);
                return std::make_shared<ASTVarExpr>(ActiveNamingScheme().stack_local_name(abs_offset), 0);
            }

            std::shared_ptr<ASTExpr> addr_expr = nullptr;
            if (!mem.base.empty()) {
                addr_expr = std::make_shared<ASTVarExpr>(mem.base, ver);
            }

            if (mem.displacement != 0 || !addr_expr) {
                auto disp_expr = std::make_shared<ASTLiteralExpr>(std::abs(mem.displacement));
                if (!addr_expr) {
                    addr_expr = disp_expr;
                } else if (mem.displacement > 0) {
                    addr_expr = std::make_shared<ASTBinaryExpr>("+", addr_expr, disp_expr);
                } else {
                    addr_expr = std::make_shared<ASTBinaryExpr>("-", addr_expr, disp_expr);
                }
            }
            return std::make_shared<ASTMemoryExpr>(addr_expr);
        } else if (std::holds_alternative<std::string>(op.value)) {
            return std::make_shared<ASTVarExpr>(std::get<std::string>(op.value), ver);
        }

        return std::make_shared<ASTVarExpr>(ActiveNamingScheme().local_name(Register::RAX, ver), ver);
    }

    static std::shared_ptr<ASTExpr> GetReadExpr(const IROperand& op, const std::map<std::string, uint32_t>& live_versions, const ParamIndexMap& param_index) {
        if (std::holds_alternative<Register>(op.value)) {
            Register reg = std::get<Register>(op.value);
            std::string reg_name = (reg == Register::Unknown)
                ? ActiveNamingScheme().local_name(Register::RAX, 0)
                : ActiveNamingScheme().local_name(reg, 0);

            auto it = live_versions.find(reg_name);
            if (it != live_versions.end() && it->second != 0) {
                return std::make_shared<ASTVarExpr>(reg_name, it->second);
            }

            /*
             * we don't have an entry in live_versions yet (e.g. first read in a new
             * branch scope, or a live-in register with no prior write).
             *
             * if this is an RMW operand, op.ssa_version is already set to the NEW
             * version this instruction creates. Falling through to OperandToExpr
             * would accidentally read that new version and output self-referential
             * garbage like "x_v2 = x_v2 - 1".
             *
             * grab pre_write_version instead, which IRLifter::RenameBlockDFS recorded
             * as the version live right before this write. Note: 0 is a valid
             * version, so check against UINT32_MAX to see if it's actually an RMW target.
             */
            if (reg != Register::Unknown && op.pre_write_version != UINT32_MAX) {
                return std::make_shared<ASTVarExpr>(reg_name, op.pre_write_version);
            }
        }

        /*
         * no live (post-write) version found -> either a parameter's first read,
         * or an ordinary unversioned local; OperandToExpr handles both
         */
        return OperandToExpr(op, param_index);
    }

    static void UpdateLiveVersion(const IROperand& dest_op, std::map<std::string, uint32_t>& live_versions) {
        if (std::holds_alternative<Register>(dest_op.value)) {
            Register reg = std::get<Register>(dest_op.value);
            std::string reg_name = (reg == Register::Unknown)
                ? ActiveNamingScheme().local_name(Register::RAX, 0)
                : ActiveNamingScheme().local_name(reg, 0);
            if (dest_op.ssa_version > 0) {
                live_versions[reg_name] = dest_op.ssa_version;
            }
        }
    }

    struct BlockASTResult {
        std::shared_ptr<ASTBlockStmt> block_ast;
        std::shared_ptr<ASTExpr> last_cmp_lhs;
        std::shared_ptr<ASTExpr> last_cmp_rhs;
    };

    static BlockASTResult ConvertBlockToAST(const BasicBlock& block, std::map<std::string, uint32_t>& live_versions, const ParamIndexMap& param_index) {
        BlockASTResult res;
        res.block_ast = std::make_shared<ASTBlockStmt>();

        for (const auto& inst : block.instructions) {
            if (inst.opcode == IROpcode::Phi || inst.opcode == IROpcode::Nop) {
                continue;
            }

            if (std::holds_alternative<Register>(inst.destination.value)) {
                Register reg = std::get<Register>(inst.destination.value);

                /* skip RSP, RBP, RFLAGS adjustments used for stack frames / flags */
                if (reg == Register::RSP || reg == Register::RBP || reg == Register::RFLAGS) {
                    continue;
                }
            }

            /* read expressions using live SSA versions before destination write */
            auto read_dest_expr = GetReadExpr(inst.destination, live_versions, param_index);
            auto src_expr = GetReadExpr(inst.source, live_versions, param_index);

            /* write expression using newly defined SSA version */
            auto dest_expr = OperandToExpr(inst.destination, param_index);

            /* track comparison operands for branch conditions */
            if (inst.opcode == IROpcode::Compare || inst.opcode == IROpcode::Test) {
                res.last_cmp_lhs = read_dest_expr;
                res.last_cmp_rhs = (inst.opcode == IROpcode::Test)
                    ? std::make_shared<ASTLiteralExpr>(0)
                    : src_expr;
                continue;
            }

            /* return statement */
            if (inst.opcode == IROpcode::Return) {
                std::shared_ptr<ASTExpr> ret_val = nullptr;

                if (std::holds_alternative<Register>(inst.destination.value) &&
                    std::get<Register>(inst.destination.value) != Register::Unknown) {
                    ret_val = GetReadExpr(inst.destination, live_versions, param_index);
                } else if (std::holds_alternative<Register>(inst.source.value) && std::get<Register>(inst.source.value) != Register::Unknown) {
                    ret_val = GetReadExpr(inst.source, live_versions, param_index);
                } else {
                    /*
                     * bare `ret` has no operand telling us what's being returned.
                     * Scan every width of the accumulator register family (rax,
                     * eax, ax, al) for whichever one was actually last written,
                     * hardcoding Register::RAX here always misses for 32-bit code,
                     * since nothing ever writes "rax", only "eax".
                     */
                    static const std::array<Register, 4> kAccumulatorWidths = {
                        Register::RAX, Register::EAX, Register::AX, Register::AL
                    };
                    std::string found_name;
                    uint32_t found_ver = 0;
                    for (Register r : kAccumulatorWidths) {
                        std::string name = ActiveNamingScheme().local_name(r, 0);
                        auto it = live_versions.find(name);
                        if (it != live_versions.end()) {
                            found_name = name;
                            found_ver = it->second;
                            break;
                        }
                    }
                    if (found_name.empty()) {
                        found_name = ActiveNamingScheme().local_name(Register::EAX, 0);
                    }
                    ret_val = std::make_shared<ASTVarExpr>(found_name, found_ver);
                }

                res.block_ast->statements.push_back(std::make_shared<ASTReturnStmt>(ret_val));
                continue;
            }

            /* function call */
            if (inst.opcode == IROpcode::Call) {
                auto callee_expr = OperandToExpr(inst.destination, param_index);

                /*
                 * forward whatever's currently live in the SysV integer argument
                 * registers as the call's arguments. This is a heuristic, not a
                 * real call-site analysis: it assumes the callee uses the same
                 * convention and that every argument register still holds a
                 * live, relevant value at the call site (true for simple
                 * pass-through cases, not guaranteed once real register
                 * allocation/reuse is involved).
                 */
                std::vector<std::shared_ptr<ASTExpr>> call_args;
                for (Register arg_reg : IRLifter::SysVIntArgOrder()) {
                    std::string name = ActiveNamingScheme().local_name(arg_reg, 0);
                    auto it = live_versions.find(name);
                    bool is_param = param_index.count(arg_reg) != 0;
                    if (it != live_versions.end()) {
                        call_args.push_back(std::make_shared<ASTVarExpr>(name, it->second));
                    } else if (is_param) {
                        call_args.push_back(std::make_shared<ASTVarExpr>(
                            ActiveNamingScheme().param_name(param_index.at(arg_reg)), 0));
                    } else {
                        break; /* stop at the first register with no known live value */
                    }
                }

                auto call_expr = std::make_shared<ASTCallExpr>(callee_expr, call_args);
                std::string ret_name = ActiveNamingScheme().local_name(Register::RAX, 0);
                auto ret_var = std::make_shared<ASTVarExpr>(ret_name, inst.destination.ssa_version);
                res.block_ast->statements.push_back(std::make_shared<ASTAssignmentStmt>(ret_var, call_expr));
                UpdateLiveVersion(inst.destination, live_versions);
                continue;
            }

            /* binary & unary expression operations */
            std::shared_ptr<ASTExpr> rhs = nullptr;

            switch (inst.opcode) {
                case IROpcode::Add: case IROpcode::FloatAdd: case IROpcode::VectorAdd:
                    rhs = std::make_shared<ASTBinaryExpr>("+", read_dest_expr, src_expr); break;
                case IROpcode::Sub: case IROpcode::FloatSub: case IROpcode::VectorSub:
                    rhs = std::make_shared<ASTBinaryExpr>("-", read_dest_expr, src_expr); break;
                case IROpcode::Mul: case IROpcode::FloatMul: case IROpcode::VectorMul:
                    rhs = std::make_shared<ASTBinaryExpr>("*", read_dest_expr, src_expr); break;
                case IROpcode::Div: case IROpcode::FloatDiv: case IROpcode::VectorDiv:
                    rhs = std::make_shared<ASTBinaryExpr>("/", read_dest_expr, src_expr); break;
                case IROpcode::And:
                    rhs = std::make_shared<ASTBinaryExpr>("&", read_dest_expr, src_expr); break;
                case IROpcode::Or:
                    rhs = std::make_shared<ASTBinaryExpr>("|", read_dest_expr, src_expr); break;
                case IROpcode::Xor: case IROpcode::VectorLogical:
                    rhs = std::make_shared<ASTBinaryExpr>("^", read_dest_expr, src_expr); break;
                case IROpcode::Shl:
                    rhs = std::make_shared<ASTBinaryExpr>("<<", read_dest_expr, src_expr); break;
                case IROpcode::Shr:
                    rhs = std::make_shared<ASTBinaryExpr>(">>", read_dest_expr, src_expr); break;
                case IROpcode::Inc:
                    rhs = std::make_shared<ASTBinaryExpr>("+", read_dest_expr, std::make_shared<ASTLiteralExpr>(1)); break;
                case IROpcode::Dec:
                    rhs = std::make_shared<ASTBinaryExpr>("-", read_dest_expr, std::make_shared<ASTLiteralExpr>(1)); break;
                case IROpcode::Neg:
                    rhs = std::make_shared<ASTUnaryExpr>("-", read_dest_expr); break;
                case IROpcode::Not:
                    rhs = std::make_shared<ASTUnaryExpr>("~", read_dest_expr); break;
                case IROpcode::Assign: case IROpcode::VectorAssign:
                    rhs = src_expr; break;
                default:
                    continue;
            }

            if (rhs) {
                res.block_ast->statements.push_back(std::make_shared<ASTAssignmentStmt>(dest_expr, rhs));
                UpdateLiveVersion(inst.destination, live_versions);
            }
        }

        return res;
    }

    static size_t FindJoinBlock(size_t block_id, const ControlFlowGraph& cfg) {
        if (block_id >= cfg.blocks.size()) return std::numeric_limits<size_t>::max();
        const auto& block = cfg.blocks[block_id];
        if (block.successors.size() != 2) return std::numeric_limits<size_t>::max();

        auto walk_chain = [&](size_t start) {
            std::vector<size_t> chain;
            std::set<size_t> seen;
            size_t cur = start;
            while (cur < cfg.blocks.size() && seen.insert(cur).second) {
                chain.push_back(cur);
                if (cfg.blocks[cur].successors.size() != 1) break;
                cur = cfg.blocks[cur].successors[0];
            }
            return chain;
        };

        auto chain1 = walk_chain(block.successors[0]);
        auto chain2 = walk_chain(block.successors[1]);

        std::set<size_t> set2(chain2.begin(), chain2.end());
        for (size_t b : chain1) {
            if (set2.count(b)) return b;
        }
        return std::numeric_limits<size_t>::max();
    }

    /* generic CFG structurer (handles Loops, Branches, and Sequences recursively) */
    static std::shared_ptr<ASTBlockStmt> StructureCFG(size_t block_id, const ControlFlowGraph& cfg, std::set<size_t>& visited, std::map<std::string, uint32_t>& live_versions, const ParamIndexMap& param_index) {
        auto root = std::make_shared<ASTBlockStmt>();
        if (block_id >= cfg.blocks.size() || visited.count(block_id)) return root;

        visited.insert(block_id);
        const auto& block = cfg.blocks[block_id];

        /* append current basic block instructions */
        auto block_res = ConvertBlockToAST(block, live_versions, param_index);
        for (auto& stmt : block_res.block_ast->statements) {
            root->statements.push_back(stmt);
        }

        /* loop detection */
        bool is_loop_header = false;
        for (size_t candidate = 0; candidate < cfg.blocks.size(); ++candidate) {
            const auto& doms = cfg.blocks[candidate].dominators;
            bool header_dominates_candidate =
                std::find(doms.begin(), doms.end(), block_id) != doms.end();
            if (!header_dominates_candidate) continue;

            for (size_t succ : cfg.blocks[candidate].successors) {
                if (succ == block_id && IRLifter::IsBackEdge(cfg, candidate, block_id)) {
                    is_loop_header = true;
                    break;
                }
            }
            if (is_loop_header) break;
        }

        if (is_loop_header && block.successors.size() == 2) {
            /*
             * standard "while" shape: the header's condition decides whether to
             * enter the body or exit the loop. Of the two successors, the one
             * that can reach a block with a back edge to this header is the
             * body entry; the other is the exit.
             */
            auto can_reach_back_edge = [&](size_t start) {
                std::set<size_t> seen;
                std::vector<size_t> stack{start};
                while (!stack.empty()) {
                    size_t cur = stack.back();
                    stack.pop_back();
                    if (!seen.insert(cur).second) continue;
                    if (cur >= cfg.blocks.size()) continue;
                    for (size_t s : cfg.blocks[cur].successors) {
                        if (s == block_id && IRLifter::IsBackEdge(cfg, cur, block_id)) return true;
                        stack.push_back(s);
                    }
                }
                return false;
            };

            size_t succ0 = block.successors[0];
            size_t succ1 = block.successors[1];
            bool succ0_is_body = can_reach_back_edge(succ0);
            size_t body_entry = succ0_is_body ? succ0 : succ1;
            size_t exit_block = succ0_is_body ? succ1 : succ0;

            auto while_stmt = std::make_shared<ASTLoopStmt>();

            ConditionCode cond = ConditionCode::NE;
            if (!block.instructions.empty()) {
                cond = block.instructions.back().condition;
            }

            /*
             * the header's own condition branches to `exit_block` when the loop
             * should stop; the while-condition (continue looping) is therefore
             * the LOGICAL NEGATION of that branch's condition when succ0 (the
             * first/"taken" successor slot) is the exit, and the branch
             * condition as-is when succ0 is the body. Concretely: successors[0]
             * is conventionally the "condition true" target and successors[1]
             * the fallthrough, so if successors[0] == exit_block, the true-branch
             * exits the loop, meaning the loop continues on the FALSE case.
             */
            bool taken_branch_exits = (succ0 == exit_block);
            std::string cond_str = taken_branch_exits
                ? NegateConditionString(ConditionCodeToString(cond))
                : ConditionCodeToString(cond);

            while_stmt->condition = std::make_shared<ASTBinaryExpr>(
                cond_str,
                block_res.last_cmp_lhs ? block_res.last_cmp_lhs : std::make_shared<ASTVarExpr>("loop_cond", 0),
                block_res.last_cmp_rhs ? block_res.last_cmp_rhs : std::make_shared<ASTLiteralExpr>(0)
            );

            std::set<size_t> loop_visited = visited;
            loop_visited.insert(block_id);
            auto loop_live = live_versions;
            while_stmt->body = StructureCFG(body_entry, cfg, loop_visited, loop_live, param_index);
            root->statements.push_back(while_stmt);

            if (exit_block < cfg.blocks.size() && !visited.count(exit_block)) {
                auto exit_ast = StructureCFG(exit_block, cfg, visited, live_versions, param_index);
                for (auto& stmt : exit_ast->statements) {
                    root->statements.push_back(stmt);
                }
            }
            return root;
        }

        /* branching detection */
        if (block.successors.size() == 2) {
            size_t then_id = block.successors[0];
            size_t else_id = block.successors[1];

            auto if_stmt = std::make_shared<ASTIfElseStmt>();

            ConditionCode cond = ConditionCode::NE;
            if (!block.instructions.empty()) {
                cond = block.instructions.back().condition;
            }

            /* extract tracked comparison operands or fallback to block defaults */
            auto cond_lhs = block_res.last_cmp_lhs
                ? block_res.last_cmp_lhs
                : std::make_shared<ASTVarExpr>(ActiveNamingScheme().local_name(Register::RAX, 1), 1);

            auto cond_rhs = block_res.last_cmp_rhs
                ? block_res.last_cmp_rhs
                : std::make_shared<ASTLiteralExpr>(5);

            if_stmt->condition = std::make_shared<ASTBinaryExpr>(
                ConditionCodeToString(cond),
                cond_lhs,
                cond_rhs
            );

            size_t join_id = FindJoinBlock(block_id, cfg);

            std::set<size_t> then_visited = visited;
            std::set<size_t> else_visited = visited;
            if (join_id != std::numeric_limits<size_t>::max()) {
                then_visited.insert(join_id);
                else_visited.insert(join_id);
            }

            auto then_live = live_versions;
            auto else_live = live_versions;

            if_stmt->then_branch = StructureCFG(then_id, cfg, then_visited, then_live, param_index);
            if_stmt->else_branch = StructureCFG(else_id, cfg, else_visited, else_live, param_index);

            if (join_id != std::numeric_limits<size_t>::max()) {
                AppendPhiCopiesForEdge(if_stmt->then_branch, cfg, join_id, then_id);
                AppendPhiCopiesForEdge(if_stmt->else_branch, cfg, join_id, else_id);

                /*
                 * seed live_versions (the map used for code AFTER the join) with
                 * each phi's merged version, so a read right after the branch,
                 * with no intervening write, resolves to "whichever branch's
                 * value, now unified under the merged name" instead of the
                 * stale pre-branch version that live_versions still held.
                 */
                if (join_id < cfg.blocks.size()) {
                    for (const auto& inst : cfg.blocks[join_id].instructions) {
                        if (inst.opcode != IROpcode::Phi) continue;
                        if (!std::holds_alternative<Register>(inst.destination.value)) continue;
                        Register reg = std::get<Register>(inst.destination.value);
                        if (reg == Register::Unknown || inst.destination.ssa_version == 0) continue;
                        std::string name = ActiveNamingScheme().local_name(reg, 0);
                        live_versions[name] = inst.destination.ssa_version;
                    }
                }
            }

            root->statements.push_back(if_stmt);

            if (join_id != std::numeric_limits<size_t>::max() && !visited.count(join_id)) {
                auto join_ast = StructureCFG(join_id, cfg, visited, live_versions, param_index);
                for (auto& stmt : join_ast->statements) {
                    root->statements.push_back(stmt);
                }
            }
        }

        /* sequential Fall-through */
        else if (block.successors.size() == 1) {
            size_t succ_id = block.successors[0];
            if (!visited.count(succ_id)) {
                auto next_ast = StructureCFG(succ_id, cfg, visited, live_versions, param_index);
                for (auto& stmt : next_ast->statements) {
                    root->statements.push_back(stmt);
                }
            }
        }

        return root;
    }

    std::shared_ptr<ASTBlockStmt> ASTBuilder::BuildAST(const ControlFlowGraph& cfg) {
        if (cfg.blocks.empty()) return std::make_shared<ASTBlockStmt>();
        std::set<size_t> visited;
        std::map<std::string, uint32_t> live_versions;

        std::set<Register> live_in = IRLifter::FindUpwardExposedRegisters(cfg);
        ParamIndexMap param_index = IRLifter::BuildParamIndexMap(live_in);

        return StructureCFG(0, cfg, visited, live_versions, param_index);
    }
}
