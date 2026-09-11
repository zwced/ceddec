#pragma once
#include <ceddec/types.hpp>

namespace ceddec {
    struct CEDDEC_API ParserConfig {
        Architecture arch = Architecture::x86_64;
        bool intel_syntax = true;
        bool strict_mode = false;             /* fail explicitly on unknown structures */
        bool default_hex_immediates = false;  /* parse immediates as hex even without 0x */

        /* at&t-specific knobs, ignored entirely when intel_syntax is true */
        bool att_require_size_suffix = false; /* reject sizable mnemonics with no b/w/l/q suffix */
        bool att_allow_star_indirect = true;  /* accept "*%rax" / "*0x10(%rax)" indirect call/jmp targets */

        bool allow_unknown_mnemonics = false;  /* keep is_valid=true for unmapped opcodes, instead of rejecting them */
        size_t max_operand_count = 8;          /* drop operands past this many per instruction (0 = unlimited) */
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
