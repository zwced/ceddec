#include <ceddec/formatter.hpp>

namespace ceddec {
    std::string FormatFunctionHeader(const ceddec::FunctionSignature& sig) {
        std::string param_list;
        if (sig.params.empty()) {
            param_list = "void";
        } else {
            for (size_t i = 0; i < sig.params.size(); ++i) {
                param_list += sig.params[i];
                if (i + 1 < sig.params.size()) param_list += ", ";
            }
        }
        return sig.name + "(" + param_list + ")";
    }
}
