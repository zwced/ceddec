#include <ceddec/parser.hpp>
#include <ceddec/types.hpp>
#include <source/misc/opcode.hpp>
#include <charconv>
#include <stdexcept>
#include <unordered_map>

#include <tools.hpp>

namespace {
    using namespace ceddec;

    static std::string_view StripSizeKeywords(std::string_view str) {
        const std::string_view prefixes[] = {
            "dword ptr", "qword ptr", "word ptr", "byte ptr", "ptr"
        };

        for (auto prefix : prefixes) {
            if (str.starts_with(prefix)) {
                str.remove_prefix(prefix.size());
                str = Trim(str);
            }
        }
        return str;
    }

    static bool ParseInteger(std::string_view str, int64_t& out_val, bool default_hex) {
        str = Trim(str);
        if (str.empty()) return false;

        bool is_negative = str.starts_with('-');
        if (is_negative || str.starts_with('+')) {
            str.remove_prefix(1);
        }

        int base_num = default_hex ? 16 : 10;
        if (str.starts_with("0x") || str.starts_with("0X")) {
            str.remove_prefix(2);
            base_num = 16;
        }

        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out_val, base_num);
        if (ec == std::errc{}) {
            if (is_negative) out_val = -out_val;
            return true;
        }
        return false;
    }

    /*
     * splits operands_text on top-level commas only, depth tracks any bracket/paren nesting
     * so memory expressions like "(%rax,%rbx,4)" or "[x0, x1]" aren't split internally
     * pass open=close='\0' when the caller has already isolated a bracket-free fragment
     */
    static std::vector<std::string_view> SplitTopLevel(std::string_view operands_text, char open, char close) {
        std::vector<std::string_view> tokens;
        size_t start = 0;
        int depth = 0;
        for (size_t idx = 0; idx <= operands_text.size(); ++idx) {
            if (idx == operands_text.size() || (operands_text[idx] == ',' && depth == 0)) {
                std::string_view tok = Trim(operands_text.substr(start, idx - start));
                if (!tok.empty()) tokens.push_back(tok);
                start = idx + 1;
            } else {
                if (open != '\0' && operands_text[idx] == open) depth++;
                else if (close != '\0' && operands_text[idx] == close) depth--;
            }
        }
        return tokens;
    }

    /*
     * condition-code suffixes shared by jcc / setcc / cmovcc / loopcc, keyed by the
     * mnemonic tail once the fixed prefix ("j", "set", "cmov", "loop") is stripped
     */
    static const std::unordered_map<std::string_view, ConditionCode> kX86ConditionSuffixes = {
        {"o",   ConditionCode::O},   {"no",  ConditionCode::NO},
        {"b",   ConditionCode::B},   {"c",   ConditionCode::B},   {"nae", ConditionCode::B},
        {"ae",  ConditionCode::AE},  {"nb",  ConditionCode::AE},  {"nc",  ConditionCode::AE},
        {"e",   ConditionCode::E},   {"z",   ConditionCode::E},
        {"ne",  ConditionCode::NE},  {"nz",  ConditionCode::NE},
        {"be",  ConditionCode::BE},  {"na",  ConditionCode::BE},
        {"a",   ConditionCode::A},   {"nbe", ConditionCode::A},
        {"s",   ConditionCode::S},   {"ns",  ConditionCode::NS},
        {"p",   ConditionCode::P},   {"pe",  ConditionCode::P},
        {"np",  ConditionCode::NP},  {"po",  ConditionCode::NP},
        {"l",   ConditionCode::L},   {"nge", ConditionCode::L},
        {"ge",  ConditionCode::GE},  {"nl",  ConditionCode::GE},
        {"le",  ConditionCode::LE},  {"ng",  ConditionCode::LE},
        {"g",   ConditionCode::G},   {"nle", ConditionCode::G}
    };

    static ConditionCode ParseX86ConditionSuffix(std::string_view suffix) {
        auto it = kX86ConditionSuffixes.find(suffix);
        return (it != kX86ConditionSuffixes.end()) ? it->second : ConditionCode::None;
    }

    /*
     * pulls the predicate out of the mnemonic for every x86 conditional family; "jmp" and
     * plain "loop" are deliberately excluded since they aren't actually conditional. Shared
     * by both x86 syntaxes (Intel and AT&T), the condition mnemonics themselves don't change
     * between the two, only the operand/memory syntax does
     */
    static ConditionCode DetectX86Condition(std::string_view m_lower) {
        if (m_lower.starts_with("cmov") && m_lower.size() > 4) {
            return ParseX86ConditionSuffix(m_lower.substr(4));
        }
        if (m_lower.starts_with("set") && m_lower.size() > 3) {
            return ParseX86ConditionSuffix(m_lower.substr(3));
        }
        if (m_lower.starts_with("loop") && m_lower.size() > 4) {
            return ParseX86ConditionSuffix(m_lower.substr(4));
        }
        if (m_lower.starts_with('j') && m_lower != "jmp" && m_lower.size() > 1) {
            return ParseX86ConditionSuffix(m_lower.substr(1));
        }
        return ConditionCode::None;
    }

    /*
     * shared prefix-stripping loop (lock/rep/repe/repz/repne/repnz), identical token set in
     * both x86 syntaxes, since prefixes are written as bare leading words in both.
     * Returns false (and leaves instr invalid) if a prefix is present with nothing after it.
     */
    static bool StripX86Prefixes(std::string_view& line, ParsedInstruction& instr) {
        for (;;) {
            size_t prefix_space = line.find_first_of(" \t");
            std::string_view head = (prefix_space == std::string_view::npos) ? line : line.substr(0, prefix_space);
            std::string head_lower = ToLower(head);

            if (head_lower == "lock") {
                instr.lock = true;
            } else if (head_lower == "rep" || head_lower == "repe" || head_lower == "repz") {
                instr.rep = RepPrefix::Rep;
            } else if (head_lower == "repne" || head_lower == "repnz") {
                instr.rep = RepPrefix::Repne;
            } else {
                return true;
            }

            if (prefix_space == std::string_view::npos) {
                return false; /* bare prefix with nothing after it isn't a real instruction */
            }
            line = Trim(line.substr(prefix_space + 1));
        }
    }

    /* intel syntax stuff */
    static MemoryOperand ParseIntelMemoryExpression(std::string_view raw_str, const ParserConfig& config) {
        MemoryOperand mem;

        if (raw_str.starts_with('[') && raw_str.ends_with(']')) {
            raw_str.remove_prefix(1);
            raw_str.remove_suffix(1);
        }
        raw_str = Trim(raw_str);

        size_t pos = 0;
        bool is_first = true;

        while (pos < raw_str.size()) {
            char op_sign = '+';
            if (!is_first) {
                op_sign = raw_str[pos];
                pos++;
            }
            is_first = false;

            size_t next_sign = raw_str.find_first_of("+-", pos);
            if (next_sign == std::string_view::npos) {
                next_sign = raw_str.size();
            }

            std::string_view token = Trim(raw_str.substr(pos, next_sign - pos));
            pos = next_sign;

            if (token.empty()) continue;

            if (size_t star_pos = token.find('*'); star_pos != std::string_view::npos) {
                mem.index = Trim(token.substr(0, star_pos));
                int64_t scale_val = 1;
                if (ParseInteger(token.substr(star_pos + 1), scale_val, config.default_hex_immediates)) {
                    mem.scale = static_cast<uint8_t>(scale_val);
                }
            } else {
                int64_t disp_val = 0;
                if (ParseInteger(token, disp_val, config.default_hex_immediates)) {
                    mem.displacement += (op_sign == '-') ? -disp_val : disp_val;
                } else if (mem.base.empty()) {
                    mem.base = token;
                } else if (mem.index.empty()) {
                    mem.index = token;
                }
            }
        }

        if (std::string base_lower = ToLower(mem.base); base_lower == "rip" || base_lower == "eip") {
            mem.rip_relative = true;
        }

        return mem;
    }

    static ParsedOperand ParseIntelOperandToken(std::string_view token, const ParserConfig& config) {
        ParsedOperand op;

        /* strip size modifiers ("dword ptr [rbp - 4]" -> "[rbp - 4]") */
        token = StripSizeKeywords(token);
        op.raw_text = token;

        if (token.find('[') != std::string_view::npos) {
            op.type = OperandType::Memory;
            op.mem = ParseIntelMemoryExpression(token, config);
        } else if (int64_t val = 0; ParseInteger(token, val, config.default_hex_immediates)) {
            op.type = OperandType::Immediate;
            op.immediate_val = val;
        } else {
            op.type = OperandType::Register;
        }

        return op;
    }

    ParsedInstruction ParseLineIntel(std::string_view line, const ParserConfig& config) {
        ParsedInstruction instr;

        if (size_t comment_pos = line.find(';'); comment_pos != std::string_view::npos) {
            line = line.substr(0, comment_pos);
        }
        line = Trim(line);
        if (line.empty()) return instr;

        if (line.back() == ':') {
            instr.mnemonic = "label";
            ParsedOperand op;
            op.type = OperandType::Label;
            op.raw_text = line.substr(0, line.size() - 1);
            instr.operands.push_back(op);
            instr.is_valid = true;
            return instr;
        }

        if (!StripX86Prefixes(line, instr)) {
            return instr;
        }

        /* extract mnemonic and split remaining operands string */
        size_t space_pos = line.find_first_of(" \t");
        std::string_view mnemonic_str = (space_pos == std::string_view::npos) ? line : Trim(line.substr(0, space_pos));
        std::string_view operand_str = (space_pos != std::string_view::npos) ? Trim(line.substr(space_pos + 1)) : "";

        instr.mnemonic = mnemonic_str;

        /* parse operands respecting bracket nesting */
        if (!operand_str.empty()) {
            for (std::string_view tok : SplitTopLevel(operand_str, '[', ']')) {
                instr.operands.push_back(ParseIntelOperandToken(tok, config));
            }
        }

        if (config.max_operand_count != 0 && instr.operands.size() > config.max_operand_count) {
            instr.operands.resize(config.max_operand_count);
        }

        instr.condition = DetectX86Condition(ToLower(instr.mnemonic));
        bool opcode_known = MapOpcode(ToLower(instr.mnemonic)) != IROpcode::Unknown;
        instr.is_valid = opcode_known || instr.condition != ConditionCode::None || instr.mnemonic == "label" || config.allow_unknown_mnemonics;

        return instr;
    }

    /* at&t stuff */

    /*
     * base mnemonics that legitimately take a b/w/l/q size suffix in GAS AT&T syntax. Needed
     * because a naive "strip trailing b/w/l/q" would mangle mnemonics like "call" (ends in 'l')
     * that were never suffixed in the first place.
     */
    static const std::unordered_map<std::string_view, int> kAttSizableMnemonics = {
        {"mov",0},{"add",0},{"sub",0},{"cmp",0},{"test",0},{"and",0},{"or",0},{"xor",0},
        {"not",0},{"neg",0},{"inc",0},{"dec",0},{"shl",0},{"shr",0},{"sal",0},{"sar",0},
        {"rol",0},{"ror",0},{"push",0},{"pop",0},{"imul",0},{"idiv",0},{"mul",0},{"div",0},
        {"adc",0},{"sbb",0},{"lea",0},{"movzx",0},{"movsx",0}
    };

    /* forms: "disp(base,index,scale)", "(base,index,scale)", "disp(base)", "symbol(%rip)" */
    static MemoryOperand ParseAttMemoryExpression(std::string_view tok, const ParserConfig& config) {
        MemoryOperand mem;

        size_t paren = tok.find('(');
        std::string_view disp_part = Trim(tok.substr(0, paren));
        std::string_view inner;
        if (paren != std::string_view::npos) {
            size_t close = tok.rfind(')');
            if (close != std::string_view::npos && close > paren) {
                inner = Trim(tok.substr(paren + 1, close - paren - 1));
            }
        }

        if (!disp_part.empty()) {
            int64_t d = 0;
            if (ParseInteger(disp_part, d, config.default_hex_immediates)) {
                mem.displacement = d;
            } else {
                /*
                 * non-numeric displacement is a symbol (e.g. "my_global(%rip)"), no dedicated
                 * symbol field on MemoryOperand, so it's carried in `base` as a fallback.
                 */
                mem.base = disp_part;
            }
        }

        if (!inner.empty()) {
            auto parts = SplitTopLevel(inner, '\0', '\0'); /* no nested brackets possible here */
            if (parts.size() >= 1 && !parts[0].empty()) {
                std::string_view b = parts[0];
                if (b.starts_with('%')) b.remove_prefix(1);
                mem.base = b;
            }
            if (parts.size() >= 2 && !parts[1].empty()) {
                std::string_view idx = parts[1];
                if (idx.starts_with('%')) idx.remove_prefix(1);
                mem.index = idx;
            }
            if (parts.size() >= 3) {
                int64_t sc = 1;
                if (ParseInteger(parts[2], sc, config.default_hex_immediates)) mem.scale = static_cast<uint8_t>(sc);
            }
        }

        if (std::string base_lower = ToLower(mem.base); base_lower == "rip" || base_lower == "eip") {
            mem.rip_relative = true;
        }
        return mem;
    }

    static ParsedInstruction ParseLineAtt(std::string_view line, const ParserConfig& config) {
        ParsedInstruction instr;

        if (size_t comment_pos = line.find(';'); comment_pos != std::string_view::npos) {
            line = line.substr(0, comment_pos);
        }
        line = Trim(line);
        if (line.empty()) return instr;

        if (line.back() == ':') {
            instr.mnemonic = "label";
            ParsedOperand op;
            op.type = OperandType::Label;
            op.raw_text = line.substr(0, line.size() - 1);
            instr.operands.push_back(op);
            instr.is_valid = true;
            return instr;
        }

        if (!StripX86Prefixes(line, instr)) {
            return instr;
        }

        size_t space_pos = line.find_first_of(" \t");
        std::string_view raw_mnemonic = (space_pos == std::string_view::npos) ? line : Trim(line.substr(0, space_pos));
        std::string_view operands_text = (space_pos == std::string_view::npos) ? std::string_view{} : Trim(line.substr(space_pos + 1));

        std::string mnem_lower_full = ToLower(raw_mnemonic);
        uint16_t suffix_width = 0;
        bool had_size_suffix = false;

        std::string_view mnemonic = raw_mnemonic;
        std::string_view sizable_stem; /* stem of a sizable mnemonic even when no suffix was found */

        if (!mnem_lower_full.empty()) {
            char last = mnem_lower_full.back();
            uint16_t w = (last == 'b') ? 8 : (last == 'w') ? 16 : (last == 'l') ? 32 : (last == 'q') ? 64 : 0;

            if (w != 0) {
                std::string_view stem(mnem_lower_full.data(), mnem_lower_full.size() - 1);
                if (kAttSizableMnemonics.count(stem) ||
                    MapOpcode(stem) != IROpcode::Unknown) {
                    suffix_width = w;
                    had_size_suffix = true;
                    mnemonic = raw_mnemonic.substr(0, raw_mnemonic.size() - 1);
                }
            }

            if (!had_size_suffix && kAttSizableMnemonics.count(mnem_lower_full)) {
                sizable_stem = mnemonic; /* suffix-less but still a sizable base mnemonic */
            }
        }
        instr.mnemonic = mnemonic;

        /* strict mode + att_require_size_suffix: a sizable mnemonic with no b/w/l/q is ambiguous */
        if (config.strict_mode && config.att_require_size_suffix && !had_size_suffix && !sizable_stem.empty()) {
            return instr; /* is_valid stays false */
        }

        if (operands_text.empty()) {
            instr.condition = DetectX86Condition(ToLower(instr.mnemonic));
            instr.is_valid = (MapOpcode(ToLower(instr.mnemonic)) != IROpcode::Unknown) || instr.condition != ConditionCode::None;
            return instr;
        }

        std::vector<ParsedOperand> ops;
        for (std::string_view tok : SplitTopLevel(operands_text, '(', ')')) {
            ParsedOperand op;
            op.raw_text = tok;

            if (tok.starts_with('*')) {
                if (!config.att_allow_star_indirect) continue; /* drop disallowed indirect-target token */
                tok.remove_prefix(1); /* indirect call/jmp marker */
            }

            if (tok.starts_with("%fs:"))      { op.segment = SegmentReg::Fs; tok.remove_prefix(4); }
            else if (tok.starts_with("%gs:")) { op.segment = SegmentReg::Gs; tok.remove_prefix(4); }
            else if (tok.starts_with("%cs:")) { op.segment = SegmentReg::Cs; tok.remove_prefix(4); }
            else if (tok.starts_with("%ds:")) { op.segment = SegmentReg::Ds; tok.remove_prefix(4); }
            else if (tok.starts_with("%es:")) { op.segment = SegmentReg::Es; tok.remove_prefix(4); }
            else if (tok.starts_with("%ss:")) { op.segment = SegmentReg::Ss; tok.remove_prefix(4); }

            if (tok.starts_with('$')) {
                op.type = OperandType::Immediate;
                ParseInteger(tok.substr(1), op.immediate_val, config.default_hex_immediates);
            } else if (tok.starts_with('%')) {
                /* at&t register sigil isn't part of the register name itself, strip it so
                 * ParseRegister() (which expects bare "rax", not "%rax") can resolve it */
                op.type = OperandType::Register;
                tok.remove_prefix(1);
                op.raw_text = tok;
            } else if (tok.find('(') != std::string_view::npos) {
                op.type = OperandType::Memory;
                op.mem = ParseAttMemoryExpression(tok, config);
                op.bit_width = suffix_width;
            } else if (mnemonic.starts_with('j') || mnemonic == "call") {
                /* bare branch/call target with no % $ ( ) prefix is a symbolic label */
                op.type = OperandType::Label;
            } else if (int64_t dummy; ParseInteger(tok, dummy, config.default_hex_immediates)) {
                /*
                 * AT&T requires '$' for immediates, so a bare number with no prefix is an
                 * absolute memory address, not an immediate.
                 */
                op.type = OperandType::Memory;
                op.mem.displacement = dummy;
                op.bit_width = suffix_width;
            } else {
                /*
                 * bare symbol with no register sigil, AT&T has no unadorned-register syntax,
                 * so this is a symbolic memory reference (e.g. a global variable name).
                 */
                op.type = OperandType::Memory;
                op.mem.base = tok;
                op.bit_width = suffix_width;
            }

            ops.push_back(op);
        }

        /*
         * AT&T operand order is src...,dst, reverse so operands[0] is always the destination,
         * matching the convention the rest of this codebase (and Intel syntax) already uses.
         */
        std::reverse(ops.begin(), ops.end());
        if (config.max_operand_count != 0 && ops.size() > config.max_operand_count) {
            ops.resize(config.max_operand_count);
        }
        instr.operands = std::move(ops);

        instr.condition = DetectX86Condition(ToLower(instr.mnemonic));
        bool opcode_known = MapOpcode(ToLower(instr.mnemonic)) != IROpcode::Unknown;
        instr.is_valid = opcode_known || instr.condition != ConditionCode::None || config.allow_unknown_mnemonics;
        return instr;
    }
}

namespace ceddec {
    Parser CreateParser(ParserConfig config) {
        return std::make_shared<ParserImpl>(config);
    }

    ParsedInstruction ParserImpl::ParseLine(std::string_view line) {
        ParsedInstruction instr;
        if (config_.intel_syntax) {
            instr = ParseLineIntel(line, config_);
        } else {
            instr = ParseLineAtt(line, config_);
        }

        bool is_blank = Trim(line).empty();

        if (config_.strict_mode && !instr.is_valid && !is_blank) {
            throw std::runtime_error("ceddec: failed to parse instruction: \"" + std::string(line) + "\"");
        }
        return instr;
    }

    ParsedBlock ParserImpl::ParseBlock(std::string_view block_text) {
        ParsedBlock block;
        size_t start = 0;

        while (start <= block_text.size()) {
            size_t newline_pos = block_text.find('\n', start);
            std::string_view line = (newline_pos == std::string_view::npos)
                ? block_text.substr(start)
                : block_text.substr(start, newline_pos - start);

            ParsedInstruction instr = ParseLine(line);
            if (instr.is_valid) {
                block.instructions.push_back(instr);
            }

            if (newline_pos == std::string_view::npos) {
                break;
            }
            start = newline_pos + 1;
        }

        return block;
    }
}
