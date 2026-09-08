#pragma once
#include <ceddec/ast.hpp>
#include <sstream>
#include <string>
#include <memory>

namespace ceddec {
    class CEDDEC_API BaseEmitter : public ASTVisitor {
    public:
        explicit BaseEmitter(int indent_spaces = 4);
        virtual ~BaseEmitter() = default;

        virtual std::string Emit(ASTStmt& root);
        virtual std::string EmitExpr(ASTExpr& expr);

    protected:
        std::string Indent() const;
        void PushIndent();
        void PopIndent();

        std::ostringstream out_;
        std::string last_expr_;
        int indent_level_ = 0;
        int indent_spaces_ = 4;
    };
}
