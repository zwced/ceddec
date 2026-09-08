#pragma once
#include <ceddec/types.hpp>
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace ceddec {
    struct ASTVarExpr;
    struct ASTLiteralExpr;
    struct ASTBinaryExpr;
    struct ASTUnaryExpr;
    struct ASTMemoryExpr;
    struct ASTCallExpr;
    struct ASTBlockStmt;
    struct ASTAssignmentStmt;
    struct ASTIfElseStmt;
    struct ASTLoopStmt;
    struct ASTReturnStmt;

    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;
        virtual void visit(ASTVarExpr& node) = 0;
        virtual void visit(ASTLiteralExpr& node) = 0;
        virtual void visit(ASTBinaryExpr& node) = 0;
        virtual void visit(ASTUnaryExpr& node) = 0;
        virtual void visit(ASTMemoryExpr& node) = 0;
        virtual void visit(ASTCallExpr& node) = 0;
        virtual void visit(ASTBlockStmt& node) = 0;
        virtual void visit(ASTAssignmentStmt& node) = 0;
        virtual void visit(ASTIfElseStmt& node) = 0;
        virtual void visit(ASTLoopStmt& node) = 0;
        virtual void visit(ASTReturnStmt& node) = 0;
    };

    /* expressions */

    enum class ASTExprType { Variable, Literal, BinaryOp, UnaryOp, MemoryAccess, Call };

    struct ASTExpr {
        ASTExprType expr_type;
        virtual ~ASTExpr() = default;
        virtual void accept(ASTVisitor& visitor) = 0;
    };

    struct ASTVarExpr : public ASTExpr {
        std::string name;
        uint32_t ssa_version = 0;

        ASTVarExpr(std::string_view var_name, uint32_t ver = 0): name(var_name), ssa_version(ver) {}

        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTLiteralExpr : public ASTExpr {
        uint64_t value;
        ASTLiteralExpr(uint64_t val) : value(val) { expr_type = ASTExprType::Literal; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTBinaryExpr : public ASTExpr {
        std::string op;
        std::shared_ptr<ASTExpr> lhs;
        std::shared_ptr<ASTExpr> rhs;
        ASTBinaryExpr(std::string binary_op, std::shared_ptr<ASTExpr> l, std::shared_ptr<ASTExpr> r)
            : op(std::move(binary_op)), lhs(std::move(l)), rhs(std::move(r)) { expr_type = ASTExprType::BinaryOp; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTUnaryExpr : public ASTExpr {
        std::string op;
        std::shared_ptr<ASTExpr> operand;
        ASTUnaryExpr(std::string unary_op, std::shared_ptr<ASTExpr> expr)
            : op(std::move(unary_op)), operand(std::move(expr)) { expr_type = ASTExprType::UnaryOp; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTMemoryExpr : public ASTExpr {
        std::shared_ptr<ASTExpr> base_address;
        int64_t offset = 0;
        ASTMemoryExpr(std::shared_ptr<ASTExpr> base, int64_t off = 0)
            : base_address(std::move(base)), offset(off) { expr_type = ASTExprType::MemoryAccess; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTCallExpr : public ASTExpr {
        std::shared_ptr<ASTExpr> callee;
        std::vector<std::shared_ptr<ASTExpr>> args;
        ASTCallExpr(std::shared_ptr<ASTExpr> fn, std::vector<std::shared_ptr<ASTExpr>> fn_args = {})
            : callee(std::move(fn)), args(std::move(fn_args)) { expr_type = ASTExprType::Call; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    /* statements */

    enum class ASTStmtType { Block, Assignment, IfElse, Loop, Return };

    struct ASTStmt {
        ASTStmtType stmt_type;
        virtual ~ASTStmt() = default;
        virtual void accept(ASTVisitor& visitor) = 0;
    };

    struct ASTBlockStmt : public ASTStmt {
        std::vector<std::shared_ptr<ASTStmt>> statements;
        ASTBlockStmt() { stmt_type = ASTStmtType::Block; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTAssignmentStmt : public ASTStmt {
        std::shared_ptr<ASTExpr> target;
        std::shared_ptr<ASTExpr> expression;
        ASTAssignmentStmt(std::shared_ptr<ASTExpr> tgt, std::shared_ptr<ASTExpr> expr)
            : target(std::move(tgt)), expression(std::move(expr)) { stmt_type = ASTStmtType::Assignment; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTIfElseStmt : public ASTStmt {
        std::shared_ptr<ASTExpr> condition;
        std::shared_ptr<ASTBlockStmt> then_branch;
        std::shared_ptr<ASTBlockStmt> else_branch;
        ASTIfElseStmt() { stmt_type = ASTStmtType::IfElse; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTLoopStmt : public ASTStmt {
        std::shared_ptr<ASTExpr> condition;
        std::shared_ptr<ASTBlockStmt> body;
        bool is_do_while = false;
        ASTLoopStmt() { stmt_type = ASTStmtType::Loop; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    struct ASTReturnStmt : public ASTStmt {
        std::shared_ptr<ASTExpr> return_value;
        ASTReturnStmt(std::shared_ptr<ASTExpr> val = nullptr)
            : return_value(std::move(val)) { stmt_type = ASTStmtType::Return; }
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };

    class CEDDEC_API ASTBuilder {
    public:
        static std::shared_ptr<ASTBlockStmt> BuildAST(const ControlFlowGraph& cfg);
    };
}
