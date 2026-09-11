#include <ceddec/naming.hpp>

namespace ceddec {
    NamingScheme& ActiveNamingScheme() {
        static NamingScheme scheme;
        return scheme;
    }
}
