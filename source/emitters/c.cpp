#include <ceddec/emitters/c.hpp>
#include <iomanip>
#include <sstream>

namespace ceddec {
    void CEmitter::visit(ASTVarExpr& node) {
        last_expr_ = (node.ssa_version > 0)
            ? node.name + "_v" + std::to_string(node.ssa_version)
            : node.name;
    }

    void CEmitter::visit(ASTLiteralExpr& node) {
        if (node.value > 9) {
            std::ostringstream ss;
            ss << "0x" << std::hex << node.value;
            last_expr_ = ss.str();
        } else {
            last_expr_ = std::to_string(node.value);
        }
    }

    void CEmitter::visit(ASTBinaryExpr& node) {
        std::string lhs = EmitExpr(*node.lhs);
        std::string rhs = EmitExpr(*node.rhs);
        last_expr_ = "(" + lhs + " " + node.op + " " + rhs + ")";
    }

    void CEmitter::visit(ASTUnaryExpr& node) {
        last_expr_ = "(" + node.op + EmitExpr(*node.operand) + ")";
    }

    void CEmitter::visit(ASTMemoryExpr& node) {
        std::string base = EmitExpr(*node.base_address);
        if (node.offset != 0) {
            last_expr_ = "*(uint64_t*)((uint8_t*)(" + base + ") + " + std::to_string(node.offset) + ")";
        } else {
            last_expr_ = "*(uint64_t*)(" + base + ")";
        }
    }

    void CEmitter::visit(ASTCallExpr& node) {
        std::string callee = EmitExpr(*node.callee);
        std::string args;
        for (size_t i = 0; i < node.args.size(); ++i) {
            args += EmitExpr(*node.args[i]);
            if (i + 1 < node.args.size()) args += ", ";
        }
        last_expr_ = callee + "(" + args + ")";
    }

    void CEmitter::visit(ASTBlockStmt& node) {
        for (auto& stmt : node.statements) {
            if (stmt) stmt->accept(*this);
        }
    }

    void CEmitter::visit(ASTAssignmentStmt& node) {
        out_ << Indent() << EmitExpr(*node.target) << " = " << EmitExpr(*node.expression) << ";\n";
    }

    void CEmitter::visit(ASTIfElseStmt& node) {
        out_ << Indent() << "if (" << EmitExpr(*node.condition) << ") {\n";
        PushIndent();
        if (node.then_branch) node.then_branch->accept(*this);
        PopIndent();

        if (node.else_branch && !node.else_branch->statements.empty()) {
            out_ << Indent() << "} else {\n";
            PushIndent();
            node.else_branch->accept(*this);
            PopIndent();
        }
        out_ << Indent() << "}\n";
    }

    void CEmitter::visit(ASTLoopStmt& node) {
        if (node.is_do_while) {
            out_ << Indent() << "do {\n";
            PushIndent();
            if (node.body) node.body->accept(*this);
            PopIndent();
            out_ << Indent() << "} while (" << EmitExpr(*node.condition) << ");\n";
        } else {
            out_ << Indent() << "while (" << EmitExpr(*node.condition) << ") {\n";
            PushIndent();
            if (node.body) node.body->accept(*this);
            PopIndent();
            out_ << Indent() << "}\n";
        }
    }

    void CEmitter::visit(ASTReturnStmt& node) {
        if (node.return_value) {
            out_ << Indent() << "return " << EmitExpr(*node.return_value) << ";\n";
        } else {
            out_ << Indent() << "return;\n";
        }
    }

    std::string CEmitter::EmitFunction(const std::string& func_name, const std::string& return_type, ASTStmt& body) {
        out_.str("");
        out_.clear();

        out_ << return_type << " " << func_name << " {\n";
        PushIndent();
        body.accept(*this);
        PopIndent();
        out_ << "}\n";

        return out_.str();
    }
}
