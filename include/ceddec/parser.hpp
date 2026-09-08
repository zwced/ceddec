#pragma once
#include <ceddec/types.hpp>
#include <memory>

namespace ceddec {
    struct CEDDEC_API ParserConfig {
        Architecture arch = Architecture::x86_64;
        bool intel_syntax = true;
        bool strict_mode = false;             /* fail explicitly on unknown structures */
        bool default_hex_immediates = false;  /* parse immediates as hex even without 0x */
    };

    class CEDDEC_API ParserImpl {
    public:
        explicit ParserImpl(ParserConfig config = {}) : config_(config) {}
        ParsedInstruction ParseLine(std::string_view line_text);
        ParsedBlock ParseBlock(std::string_view block_text);

    private:
        ParserConfig config_;
    };

    using Parser = std::shared_ptr<ParserImpl>;
    CEDDEC_API Parser CreateParser(ParserConfig config = {});
}
