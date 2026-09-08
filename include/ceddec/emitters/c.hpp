#pragma once
#include <ceddec/emitters/base.hpp>

namespace ceddec {
    class CEDDEC_API CEmitter : public BaseEmitter {
    public:
        using BaseEmitter::BaseEmitter;

        std::string EmitFunction(const std::string& func_name, const std::string& return_type, ASTStmt& body);

        void visit(ASTVarExpr& node) override;
        void visit(ASTLiteralExpr& node) override;
        void visit(ASTBinaryExpr& node) override;
        void visit(ASTUnaryExpr& node) override;
        void visit(ASTMemoryExpr& node) override;
        void visit(ASTCallExpr& node) override;

        void visit(ASTBlockStmt& node) override;
        void visit(ASTAssignmentStmt& node) override;
        void visit(ASTIfElseStmt& node) override;
        void visit(ASTLoopStmt& node) override;
        void visit(ASTReturnStmt& node) override;
    };
}
