#pragma once
#include <ceddec/types.hpp>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <cctype>

[[gnu::hot]] static inline ceddec::Register ParseRegister(std::string_view reg_str) {
    std::string upper_str(reg_str);
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    static const std::unordered_map<std::string_view, ceddec::Register> reg_map = {
        /* 64-bit */
        {"RAX", ceddec::Register::RAX}, {"RBX", ceddec::Register::RBX},
        {"RCX", ceddec::Register::RCX}, {"RDX", ceddec::Register::RDX},
        {"RSI", ceddec::Register::RSI}, {"RDI", ceddec::Register::RDI},
        {"RBP", ceddec::Register::RBP}, {"RSP", ceddec::Register::RSP},
        {"R8",  ceddec::Register::R8},  {"R9",  ceddec::Register::R9},
        {"R10", ceddec::Register::R10}, {"R11", ceddec::Register::R11},
        {"R12", ceddec::Register::R12}, {"R13", ceddec::Register::R13},
        {"R14", ceddec::Register::R14}, {"R15", ceddec::Register::R15},

        /* 32-bit */
        {"EAX", ceddec::Register::EAX}, {"EBX", ceddec::Register::EBX},
        {"ECX", ceddec::Register::ECX}, {"EDX", ceddec::Register::EDX},
        {"ESI", ceddec::Register::ESI}, {"EDI", ceddec::Register::EDI},
        {"EBP", ceddec::Register::EBP}, {"ESP", ceddec::Register::ESP},
        {"R8D", ceddec::Register::R8D}, {"R9D", ceddec::Register::R9D},
        {"R10D", ceddec::Register::R10D}, {"R11D", ceddec::Register::R11D},
        {"R12D", ceddec::Register::R12D}, {"R13D", ceddec::Register::R13D},
        {"R14D", ceddec::Register::R14D}, {"R15D", ceddec::Register::R15D},

        /* 16-bit */
        {"AX", ceddec::Register::AX}, {"BX", ceddec::Register::BX},
        {"CX", ceddec::Register::CX}, {"DX", ceddec::Register::DX},
        {"SI", ceddec::Register::SI}, {"DI", ceddec::Register::DI},
        {"BP", ceddec::Register::BP}, {"SP", ceddec::Register::SP},
        {"R8W", ceddec::Register::R8W}, {"R9W", ceddec::Register::R9W},
        {"R10W", ceddec::Register::R10W}, {"R11W", ceddec::Register::R11W},
        {"R12W", ceddec::Register::R12W}, {"R13W", ceddec::Register::R13W},
        {"R14W", ceddec::Register::R14W}, {"R15W", ceddec::Register::R15W},

        /* 8-bit */
        {"AL", ceddec::Register::AL}, {"BL", ceddec::Register::BL},
        {"CL", ceddec::Register::CL}, {"DL", ceddec::Register::DL},
        {"SIL", ceddec::Register::SIL}, {"DIL", ceddec::Register::DIL},
        {"BPL", ceddec::Register::BPL}, {"SPL", ceddec::Register::SPL},
        {"AH", ceddec::Register::AH}, {"BH", ceddec::Register::BH},
        {"CH", ceddec::Register::CH}, {"DH", ceddec::Register::DH},
        {"R8B", ceddec::Register::R8B}, {"R9B", ceddec::Register::R9B},
        {"R10B", ceddec::Register::R10B}, {"R11B", ceddec::Register::R11B},
        {"R12B", ceddec::Register::R12B}, {"R13B", ceddec::Register::R13B},
        {"R14B", ceddec::Register::R14B}, {"R15B", ceddec::Register::R15B},

        /* instruction pointer & flags */
        {"RIP", ceddec::Register::RIP}, {"EIP", ceddec::Register::EIP}, {"IP", ceddec::Register::IP},
        {"RFLAGS", ceddec::Register::RFLAGS}, {"EFLAGS", ceddec::Register::RFLAGS}, {"FLAGS", ceddec::Register::RFLAGS},

        /* segment registers */
        {"CS", ceddec::Register::CS}, {"DS", ceddec::Register::DS},
        {"ES", ceddec::Register::ES}, {"FS", ceddec::Register::FS},
        {"GS", ceddec::Register::GS}, {"SS", ceddec::Register::SS},

        /* floating point */
        {"ST0", ceddec::Register::ST0}, {"ST1", ceddec::Register::ST1},
        {"ST2", ceddec::Register::ST2}, {"ST3", ceddec::Register::ST3},
        {"ST4", ceddec::Register::ST4}, {"ST5", ceddec::Register::ST5},
        {"ST6", ceddec::Register::ST6}, {"ST7", ceddec::Register::ST7},

        /* mmx */
        {"MM0", ceddec::Register::MM0}, {"MM1", ceddec::Register::MM1},
        {"MM2", ceddec::Register::MM2}, {"MM3", ceddec::Register::MM3},
        {"MM4", ceddec::Register::MM4}, {"MM5", ceddec::Register::MM5},
        {"MM6", ceddec::Register::MM6}, {"MM7", ceddec::Register::MM7},

        /* 128-bit vector */
        {"XMM0", ceddec::Register::XMM0}, {"XMM1", ceddec::Register::XMM1},
        {"XMM2", ceddec::Register::XMM2}, {"XMM3", ceddec::Register::XMM3},
        {"XMM4", ceddec::Register::XMM4}, {"XMM5", ceddec::Register::XMM5},
        {"XMM6", ceddec::Register::XMM6}, {"XMM7", ceddec::Register::XMM7},
        {"XMM8", ceddec::Register::XMM8}, {"XMM9", ceddec::Register::XMM9},
        {"XMM10", ceddec::Register::XMM10}, {"XMM11", ceddec::Register::XMM11},
        {"XMM12", ceddec::Register::XMM12}, {"XMM13", ceddec::Register::XMM13},
        {"XMM14", ceddec::Register::XMM14}, {"XMM15", ceddec::Register::XMM15},
        {"XMM16", ceddec::Register::XMM16}, {"XMM17", ceddec::Register::XMM17},
        {"XMM18", ceddec::Register::XMM18}, {"XMM19", ceddec::Register::XMM19},
        {"XMM20", ceddec::Register::XMM20}, {"XMM21", ceddec::Register::XMM21},
        {"XMM22", ceddec::Register::XMM22}, {"XMM23", ceddec::Register::XMM23},
        {"XMM24", ceddec::Register::XMM24}, {"XMM25", ceddec::Register::XMM25},
        {"XMM26", ceddec::Register::XMM26}, {"XMM27", ceddec::Register::XMM27},
        {"XMM28", ceddec::Register::XMM28}, {"XMM29", ceddec::Register::XMM29},
        {"XMM30", ceddec::Register::XMM30}, {"XMM31", ceddec::Register::XMM31},

        /* 256-bit vector */
        {"YMM0", ceddec::Register::YMM0}, {"YMM1", ceddec::Register::YMM1},
        {"YMM2", ceddec::Register::YMM2}, {"YMM3", ceddec::Register::YMM3},
        {"YMM4", ceddec::Register::YMM4}, {"YMM5", ceddec::Register::YMM5},
        {"YMM6", ceddec::Register::YMM6}, {"YMM7", ceddec::Register::YMM7},
        {"YMM8", ceddec::Register::YMM8}, {"YMM9", ceddec::Register::YMM9},
        {"YMM10", ceddec::Register::YMM10}, {"YMM11", ceddec::Register::YMM11},
        {"YMM12", ceddec::Register::YMM12}, {"YMM13", ceddec::Register::YMM13},
        {"YMM14", ceddec::Register::YMM14}, {"YMM15", ceddec::Register::YMM15},
        {"YMM16", ceddec::Register::YMM16}, {"YMM17", ceddec::Register::YMM17},
        {"YMM18", ceddec::Register::YMM18}, {"YMM19", ceddec::Register::YMM19},
        {"YMM20", ceddec::Register::YMM20}, {"YMM21", ceddec::Register::YMM21},
        {"YMM22", ceddec::Register::YMM22}, {"YMM23", ceddec::Register::YMM23},
        {"YMM24", ceddec::Register::YMM24}, {"YMM25", ceddec::Register::YMM25},
        {"YMM26", ceddec::Register::YMM26}, {"YMM27", ceddec::Register::YMM27},
        {"YMM28", ceddec::Register::YMM28}, {"YMM29", ceddec::Register::YMM29},
        {"YMM30", ceddec::Register::YMM30}, {"YMM31", ceddec::Register::YMM31},

        /* 512-bit vector */
        {"ZMM0", ceddec::Register::ZMM0}, {"ZMM1", ceddec::Register::ZMM1},
        {"ZMM2", ceddec::Register::ZMM2}, {"ZMM3", ceddec::Register::ZMM3},
        {"ZMM4", ceddec::Register::ZMM4}, {"ZMM5", ceddec::Register::ZMM5},
        {"ZMM6", ceddec::Register::ZMM6}, {"ZMM7", ceddec::Register::ZMM7},
        {"ZMM8", ceddec::Register::ZMM8}, {"ZMM9", ceddec::Register::ZMM9},
        {"ZMM10", ceddec::Register::ZMM10}, {"ZMM11", ceddec::Register::ZMM11},
        {"ZMM12", ceddec::Register::ZMM12}, {"ZMM13", ceddec::Register::ZMM13},
        {"ZMM14", ceddec::Register::ZMM14}, {"ZMM15", ceddec::Register::ZMM15},
        {"ZMM16", ceddec::Register::ZMM16}, {"ZMM17", ceddec::Register::ZMM17},
        {"ZMM18", ceddec::Register::ZMM18}, {"ZMM19", ceddec::Register::ZMM19},
        {"ZMM20", ceddec::Register::ZMM20}, {"ZMM21", ceddec::Register::ZMM21},
        {"ZMM22", ceddec::Register::ZMM22}, {"ZMM23", ceddec::Register::ZMM23},
        {"ZMM24", ceddec::Register::ZMM24}, {"ZMM25", ceddec::Register::ZMM25},
        {"ZMM26", ceddec::Register::ZMM26}, {"ZMM27", ceddec::Register::ZMM27},
        {"ZMM28", ceddec::Register::ZMM28}, {"ZMM29", ceddec::Register::ZMM29},
        {"ZMM30", ceddec::Register::ZMM30}, {"ZMM31", ceddec::Register::ZMM31},

        /* mask registers */
        {"K0", ceddec::Register::K0}, {"K1", ceddec::Register::K1},
        {"K2", ceddec::Register::K2}, {"K3", ceddec::Register::K3},
        {"K4", ceddec::Register::K4}, {"K5", ceddec::Register::K5},
        {"K6", ceddec::Register::K6}, {"K7", ceddec::Register::K7},

        /* control registers */
        {"CR0", ceddec::Register::CR0}, {"CR1", ceddec::Register::CR1},
        {"CR2", ceddec::Register::CR2}, {"CR3", ceddec::Register::CR3},
        {"CR4", ceddec::Register::CR4}, {"CR5", ceddec::Register::CR5},
        {"CR6", ceddec::Register::CR6}, {"CR7", ceddec::Register::CR7},
        {"CR8", ceddec::Register::CR8}, {"CR9", ceddec::Register::CR9},
        {"CR10", ceddec::Register::CR10}, {"CR11", ceddec::Register::CR11},
        {"CR12", ceddec::Register::CR12}, {"CR13", ceddec::Register::CR13},
        {"CR14", ceddec::Register::CR14}, {"CR15", ceddec::Register::CR15},

        /* debug registers */
        {"DR0", ceddec::Register::DR0}, {"DR1", ceddec::Register::DR1},
        {"DR2", ceddec::Register::DR2}, {"DR3", ceddec::Register::DR3},
        {"DR4", ceddec::Register::DR4}, {"DR5", ceddec::Register::DR5},
        {"DR6", ceddec::Register::DR6}, {"DR7", ceddec::Register::DR7},
        {"DR8", ceddec::Register::DR8}, {"DR9", ceddec::Register::DR9},
        {"DR10", ceddec::Register::DR10}, {"DR11", ceddec::Register::DR11},
        {"DR12", ceddec::Register::DR12}, {"DR13", ceddec::Register::DR13},
        {"DR14", ceddec::Register::DR14}, {"DR15", ceddec::Register::DR15}
    };

    auto it = reg_map.find(upper_str);
    return (it != reg_map.end()) ? it->second : ceddec::Register::Unknown;
}
