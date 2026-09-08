#pragma once
#include <ceddec/types.hpp>

[[gnu::hot]] static inline std::string GetRegisterName(ceddec::Register reg) {
    switch (reg) {
        /* 64-bit General Purpose */
        case ceddec::Register::RAX: return "rax"; case ceddec::Register::RBX: return "rbx";
        case ceddec::Register::RCX: return "rcx"; case ceddec::Register::RDX: return "rdx";
        case ceddec::Register::RSI: return "rsi"; case ceddec::Register::RDI: return "rdi";
        case ceddec::Register::RBP: return "rbp"; case ceddec::Register::RSP: return "rsp";
        case ceddec::Register::R8:  return "r8";  case ceddec::Register::R9:  return "r9";
        case ceddec::Register::R10: return "r10"; case ceddec::Register::R11: return "r11";
        case ceddec::Register::R12: return "r12"; case ceddec::Register::R13: return "r13";
        case ceddec::Register::R14: return "r14"; case ceddec::Register::R15: return "r15";

        /* 32-bit Sub-registers */
        case ceddec::Register::EAX: return "eax"; case ceddec::Register::EBX: return "ebx";
        case ceddec::Register::ECX: return "ecx"; case ceddec::Register::EDX: return "edx";
        case ceddec::Register::ESI: return "esi"; case ceddec::Register::EDI: return "edi";
        case ceddec::Register::EBP: return "ebp"; case ceddec::Register::ESP: return "esp";
        case ceddec::Register::R8D: return "r8d"; case ceddec::Register::R9D: return "r9d";
        case ceddec::Register::R10D: return "r10d"; case ceddec::Register::R11D: return "r11d";
        case ceddec::Register::R12D: return "r12d"; case ceddec::Register::R13D: return "r13d";
        case ceddec::Register::R14D: return "r14d"; case ceddec::Register::R15D: return "r15d";

        /* 16-bit Sub-registers */
        case ceddec::Register::AX: return "ax"; case ceddec::Register::BX: return "bx";
        case ceddec::Register::CX: return "cx"; case ceddec::Register::DX: return "dx";
        case ceddec::Register::SI: return "si"; case ceddec::Register::DI: return "di";
        case ceddec::Register::BP: return "bp"; case ceddec::Register::SP: return "sp";
        case ceddec::Register::R8W: return "r8w"; case ceddec::Register::R9W: return "r9w";
        case ceddec::Register::R10W: return "r10w"; case ceddec::Register::R11W: return "r11w";
        case ceddec::Register::R12W: return "r12w"; case ceddec::Register::R13W: return "r13w";
        case ceddec::Register::R14W: return "r14w"; case ceddec::Register::R15W: return "r15w";

        /* 8-bit Sub-registers */
        case ceddec::Register::AL: return "al"; case ceddec::Register::BL: return "bl";
        case ceddec::Register::CL: return "cl"; case ceddec::Register::DL: return "dl";
        case ceddec::Register::SIL: return "sil"; case ceddec::Register::DIL: return "dil";
        case ceddec::Register::BPL: return "bpl"; case ceddec::Register::SPL: return "spl";
        case ceddec::Register::AH: return "ah"; case ceddec::Register::BH: return "bh";
        case ceddec::Register::CH: return "ch"; case ceddec::Register::DH: return "dh";
        case ceddec::Register::R8B: return "r8b"; case ceddec::Register::R9B: return "r9b";
        case ceddec::Register::R10B: return "r10b"; case ceddec::Register::R11B: return "r11b";
        case ceddec::Register::R12B: return "r12b"; case ceddec::Register::R13B: return "r13b";
        case ceddec::Register::R14B: return "r14b"; case ceddec::Register::R15B: return "r15b";

        /* Instruction Pointer & Flags */
        case ceddec::Register::RIP: return "rip"; case ceddec::Register::EIP: return "eip";
        case ceddec::Register::IP: return "ip";
        case ceddec::Register::RFLAGS: return "rflags";

        /* Segment Registers */
        case ceddec::Register::CS: return "cs"; case ceddec::Register::DS: return "ds";
        case ceddec::Register::ES: return "es"; case ceddec::Register::FS: return "fs";
        case ceddec::Register::GS: return "gs"; case ceddec::Register::SS: return "ss";

        /* Floating Point (x87) */
        case ceddec::Register::ST0: return "st0"; case ceddec::Register::ST1: return "st1";
        case ceddec::Register::ST2: return "st2"; case ceddec::Register::ST3: return "st3";
        case ceddec::Register::ST4: return "st4"; case ceddec::Register::ST5: return "st5";
        case ceddec::Register::ST6: return "st6"; case ceddec::Register::ST7: return "st7";

        /* MMX */
        case ceddec::Register::MM0: return "mm0"; case ceddec::Register::MM1: return "mm1";
        case ceddec::Register::MM2: return "mm2"; case ceddec::Register::MM3: return "mm3";
        case ceddec::Register::MM4: return "mm4"; case ceddec::Register::MM5: return "mm5";
        case ceddec::Register::MM6: return "mm6"; case ceddec::Register::MM7: return "mm7";

        /* Vector Registers - 128-bit */
        case ceddec::Register::XMM0: return "xmm0"; case ceddec::Register::XMM1: return "xmm1";
        case ceddec::Register::XMM2: return "xmm2"; case ceddec::Register::XMM3: return "xmm3";
        case ceddec::Register::XMM4: return "xmm4"; case ceddec::Register::XMM5: return "xmm5";
        case ceddec::Register::XMM6: return "xmm6"; case ceddec::Register::XMM7: return "xmm7";
        case ceddec::Register::XMM8: return "xmm8"; case ceddec::Register::XMM9: return "xmm9";
        case ceddec::Register::XMM10: return "xmm10"; case ceddec::Register::XMM11: return "xmm11";
        case ceddec::Register::XMM12: return "xmm12"; case ceddec::Register::XMM13: return "xmm13";
        case ceddec::Register::XMM14: return "xmm14"; case ceddec::Register::XMM15: return "xmm15";
        case ceddec::Register::XMM16: return "xmm16"; case ceddec::Register::XMM17: return "xmm17";
        case ceddec::Register::XMM18: return "xmm18"; case ceddec::Register::XMM19: return "xmm19";
        case ceddec::Register::XMM20: return "xmm20"; case ceddec::Register::XMM21: return "xmm21";
        case ceddec::Register::XMM22: return "xmm22"; case ceddec::Register::XMM23: return "xmm23";
        case ceddec::Register::XMM24: return "xmm24"; case ceddec::Register::XMM25: return "xmm25";
        case ceddec::Register::XMM26: return "xmm26"; case ceddec::Register::XMM27: return "xmm27";
        case ceddec::Register::XMM28: return "xmm28"; case ceddec::Register::XMM29: return "xmm29";
        case ceddec::Register::XMM30: return "xmm30"; case ceddec::Register::XMM31: return "xmm31";

        /* Vector Registers - 256-bit */
        case ceddec::Register::YMM0: return "ymm0"; case ceddec::Register::YMM1: return "ymm1";
        case ceddec::Register::YMM2: return "ymm2"; case ceddec::Register::YMM3: return "ymm3";
        case ceddec::Register::YMM4: return "ymm4"; case ceddec::Register::YMM5: return "ymm5";
        case ceddec::Register::YMM6: return "ymm6"; case ceddec::Register::YMM7: return "ymm7";
        case ceddec::Register::YMM8: return "ymm8"; case ceddec::Register::YMM9: return "ymm9";
        case ceddec::Register::YMM10: return "ymm10"; case ceddec::Register::YMM11: return "ymm11";
        case ceddec::Register::YMM12: return "ymm12"; case ceddec::Register::YMM13: return "ymm13";
        case ceddec::Register::YMM14: return "ymm14"; case ceddec::Register::YMM15: return "ymm15";
        case ceddec::Register::YMM16: return "ymm16"; case ceddec::Register::YMM17: return "ymm17";
        case ceddec::Register::YMM18: return "ymm18"; case ceddec::Register::YMM19: return "ymm19";
        case ceddec::Register::YMM20: return "ymm20"; case ceddec::Register::YMM21: return "ymm21";
        case ceddec::Register::YMM22: return "ymm22"; case ceddec::Register::YMM23: return "ymm23";
        case ceddec::Register::YMM24: return "ymm24"; case ceddec::Register::YMM25: return "ymm25";
        case ceddec::Register::YMM26: return "ymm26"; case ceddec::Register::YMM27: return "ymm27";
        case ceddec::Register::YMM28: return "ymm28"; case ceddec::Register::YMM29: return "ymm29";
        case ceddec::Register::YMM30: return "ymm30"; case ceddec::Register::YMM31: return "ymm31";

        /* Vector Registers - 512-bit */
        case ceddec::Register::ZMM0: return "zmm0"; case ceddec::Register::ZMM1: return "zmm1";
        case ceddec::Register::ZMM2: return "zmm2"; case ceddec::Register::ZMM3: return "zmm3";
        case ceddec::Register::ZMM4: return "zmm4"; case ceddec::Register::ZMM5: return "zmm5";
        case ceddec::Register::ZMM6: return "zmm6"; case ceddec::Register::ZMM7: return "zmm7";
        case ceddec::Register::ZMM8: return "zmm8"; case ceddec::Register::ZMM9: return "zmm9";
        case ceddec::Register::ZMM10: return "zmm10"; case ceddec::Register::ZMM11: return "zmm11";
        case ceddec::Register::ZMM12: return "zmm12"; case ceddec::Register::ZMM13: return "zmm13";
        case ceddec::Register::ZMM14: return "zmm14"; case ceddec::Register::ZMM15: return "zmm15";
        case ceddec::Register::ZMM16: return "zmm16"; case ceddec::Register::ZMM17: return "zmm17";
        case ceddec::Register::ZMM18: return "zmm18"; case ceddec::Register::ZMM19: return "zmm19";
        case ceddec::Register::ZMM20: return "zmm20"; case ceddec::Register::ZMM21: return "zmm21";
        case ceddec::Register::ZMM22: return "zmm22"; case ceddec::Register::ZMM23: return "zmm23";
        case ceddec::Register::ZMM24: return "zmm24"; case ceddec::Register::ZMM25: return "zmm25";
        case ceddec::Register::ZMM26: return "zmm26"; case ceddec::Register::ZMM27: return "zmm27";
        case ceddec::Register::ZMM28: return "zmm28"; case ceddec::Register::ZMM29: return "zmm29";
        case ceddec::Register::ZMM30: return "zmm30"; case ceddec::Register::ZMM31: return "zmm31";

        /* Mask Registers */
        case ceddec::Register::K0: return "k0"; case ceddec::Register::K1: return "k1";
        case ceddec::Register::K2: return "k2"; case ceddec::Register::K3: return "k3";
        case ceddec::Register::K4: return "k4"; case ceddec::Register::K5: return "k5";
        case ceddec::Register::K6: return "k6"; case ceddec::Register::K7: return "k7";

        /* Control Registers */
        case ceddec::Register::CR0: return "cr0"; case ceddec::Register::CR1: return "cr1";
        case ceddec::Register::CR2: return "cr2"; case ceddec::Register::CR3: return "cr3";
        case ceddec::Register::CR4: return "cr4"; case ceddec::Register::CR5: return "cr5";
        case ceddec::Register::CR6: return "cr6"; case ceddec::Register::CR7: return "cr7";
        case ceddec::Register::CR8: return "cr8"; case ceddec::Register::CR9: return "cr9";
        case ceddec::Register::CR10: return "cr10"; case ceddec::Register::CR11: return "cr11";
        case ceddec::Register::CR12: return "cr12"; case ceddec::Register::CR13: return "cr13";
        case ceddec::Register::CR14: return "cr14"; case ceddec::Register::CR15: return "cr15";

        /* Debug Registers */
        case ceddec::Register::DR0: return "dr0"; case ceddec::Register::DR1: return "dr1";
        case ceddec::Register::DR2: return "dr2"; case ceddec::Register::DR3: return "dr3";
        case ceddec::Register::DR4: return "dr4"; case ceddec::Register::DR5: return "dr5";
        case ceddec::Register::DR6: return "dr6"; case ceddec::Register::DR7: return "dr7";
        case ceddec::Register::DR8: return "dr8"; case ceddec::Register::DR9: return "dr9";
        case ceddec::Register::DR10: return "dr10"; case ceddec::Register::DR11: return "dr11";
        case ceddec::Register::DR12: return "dr12"; case ceddec::Register::DR13: return "dr13";
        case ceddec::Register::DR14: return "dr14"; case ceddec::Register::DR15: return "dr15";

        default: return "reg_" + std::to_string(static_cast<int>(reg));
    }
}
