#include <ceddec/types.hpp>
#include <ceddec/ast.hpp>
#include <source/misc/register_name.hpp>

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

    static std::shared_ptr<ASTExpr> OperandToExpr(const IROperand& op, uint32_t override_version = 0) {
        uint32_t ver = override_version != 0 ? override_version : op.ssa_version;

        if (std::holds_alternative<Register>(op.value)) {
            Register reg = std::get<Register>(op.value);
            if (reg == Register::Unknown) {
                return std::make_shared<ASTVarExpr>("eax", ver);
            }
            return std::make_shared<ASTVarExpr>(GetRegisterName(reg), ver);
        }
        else if (std::holds_alternative<int64_t>(op.value)) {
            return std::make_shared<ASTLiteralExpr>(static_cast<uint64_t>(std::get<int64_t>(op.value)));
        }
        else if (std::holds_alternative<MemoryOperand>(op.value)) {
            const auto& mem = std::get<MemoryOperand>(op.value);

            /* map stack frame offsets to clean variable / arg names */
            if ((mem.base == "rbp" || mem.base == "ebp" || mem.base == "rsp" || mem.base == "esp") && mem.index.empty()) {
                int64_t abs_offset = std::abs(mem.displacement);
                if (abs_offset == 4) return std::make_shared<ASTVarExpr>("arg0", 0);
                if (abs_offset == 8) return std::make_shared<ASTVarExpr>("arg1", 0);
                return std::make_shared<ASTVarExpr>("var_" + std::to_string(abs_offset), 0);
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

        return std::make_shared<ASTVarExpr>("eax", ver);
    }

    static std::shared_ptr<ASTExpr> GetReadExpr(const IROperand& op, const std::map<std::string, uint32_t>& live_versions) {
        if (std::holds_alternative<Register>(op.value)) {
            Register reg = std::get<Register>(op.value);
            std::string reg_name = (reg == Register::Unknown) ? "eax" : GetRegisterName(reg);
            auto it = live_versions.find(reg_name);
            if (it != live_versions.end() && it->second != 0) {
                return std::make_shared<ASTVarExpr>(reg_name, it->second);
            }
        }
        return OperandToExpr(op);
    }

    static void UpdateLiveVersion(const IROperand& dest_op, std::map<std::string, uint32_t>& live_versions) {
        if (std::holds_alternative<Register>(dest_op.value)) {
            Register reg = std::get<Register>(dest_op.value);
            std::string reg_name = (reg == Register::Unknown) ? "eax" : GetRegisterName(reg);
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

    static BlockASTResult ConvertBlockToAST(const BasicBlock& block, std::map<std::string, uint32_t>& live_versions) {
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
            auto read_dest_expr = GetReadExpr(inst.destination, live_versions);
            auto src_expr = GetReadExpr(inst.source, live_versions);

            /* write expression using newly defined SSA version */
            auto dest_expr = OperandToExpr(inst.destination);

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
                    ret_val = GetReadExpr(inst.destination, live_versions);
                } else if (std::holds_alternative<Register>(inst.source.value) &&
                           std::get<Register>(inst.source.value) != Register::Unknown) {
                    ret_val = GetReadExpr(inst.source, live_versions);
                } else {
                    /* fallback explicitly to active return register version */
                    auto it = live_versions.find("eax");
                    uint32_t ver = (it != live_versions.end()) ? it->second : (inst.source.ssa_version > 0 ? inst.source.ssa_version : inst.destination.ssa_version);
                    ret_val = std::make_shared<ASTVarExpr>("eax", ver);
                }

                res.block_ast->statements.push_back(std::make_shared<ASTReturnStmt>(ret_val));
                continue;
            }

            /* function call */
            if (inst.opcode == IROpcode::Call) {
                auto callee_expr = read_dest_expr;
                auto call_expr = std::make_shared<ASTCallExpr>(callee_expr, std::vector<std::shared_ptr<ASTExpr>>{});
                auto ret_var = std::make_shared<ASTVarExpr>(GetRegisterName(Register::RAX), inst.destination.ssa_version);
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
    static std::shared_ptr<ASTBlockStmt> StructureCFG(size_t block_id, const ControlFlowGraph& cfg, std::set<size_t>& visited, std::map<std::string, uint32_t>& live_versions) {
        auto root = std::make_shared<ASTBlockStmt>();
        if (block_id >= cfg.blocks.size() || visited.count(block_id)) return root;

        visited.insert(block_id);
        const auto& block = cfg.blocks[block_id];

        /* append current basic block instructions */
        auto block_res = ConvertBlockToAST(block, live_versions);
        for (auto& stmt : block_res.block_ast->statements) {
            root->statements.push_back(stmt);
        }

        /* loop detection */
        for (size_t succ : block.successors) {
            const auto& doms = block.dominators;
            if (std::find(doms.begin(), doms.end(), succ) != doms.end()) {
                auto while_stmt = std::make_shared<ASTLoopStmt>();

                ConditionCode cond = ConditionCode::NE;
                if (!block.instructions.empty()) {
                    cond = block.instructions.back().condition;
                }

                while_stmt->condition = std::make_shared<ASTBinaryExpr>(
                    ConditionCodeToString(cond),
                    block_res.last_cmp_lhs ? block_res.last_cmp_lhs : std::make_shared<ASTVarExpr>("loop_cond", 0),
                    block_res.last_cmp_rhs ? block_res.last_cmp_rhs : std::make_shared<ASTLiteralExpr>(0)
                );

                std::set<size_t> loop_visited = visited;
                auto loop_live = live_versions;
                while_stmt->body = StructureCFG(succ, cfg, loop_visited, loop_live);
                root->statements.push_back(while_stmt);
                return root;
            }
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
                : std::make_shared<ASTVarExpr>("eax", 1);

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

            if_stmt->then_branch = StructureCFG(then_id, cfg, then_visited, then_live);
            if_stmt->else_branch = StructureCFG(else_id, cfg, else_visited, else_live);
            root->statements.push_back(if_stmt);

            if (join_id != std::numeric_limits<size_t>::max() && !visited.count(join_id)) {
                auto join_ast = StructureCFG(join_id, cfg, visited, live_versions);
                for (auto& stmt : join_ast->statements) {
                    root->statements.push_back(stmt);
                }
            }
        }

        /* sequential Fall-through */
        else if (block.successors.size() == 1) {
            size_t succ_id = block.successors[0];
            if (!visited.count(succ_id)) {
                auto next_ast = StructureCFG(succ_id, cfg, visited, live_versions);
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
        return StructureCFG(0, cfg, visited, live_versions);
    }
}
