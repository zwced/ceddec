#include <ceddec/emitters/base.hpp>

namespace ceddec {
    BaseEmitter::BaseEmitter(int indent_spaces): indent_spaces_(indent_spaces) {}

    std::string BaseEmitter::Indent() const {
        return std::string(indent_level_ * indent_spaces_, ' ');
    }

    void BaseEmitter::PushIndent() {
        indent_level_++;
    }

    void BaseEmitter::PopIndent() {
        if (indent_level_ > 0) indent_level_--;
    }

    std::string BaseEmitter::Emit(ASTStmt& root) {
        out_.str("");
        out_.clear();
        root.accept(*this);
        return out_.str();
    }

    std::string BaseEmitter::EmitExpr(ASTExpr& expr) {
        last_expr_.clear();
        expr.accept(*this);
        return last_expr_;
    }
}
