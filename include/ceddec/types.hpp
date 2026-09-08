#pragma once
#include <internal.h>

#include <cstdint>
#include <string_view>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <set>
#include <map>
#include <memory>
#include <stack>
#include <cmath>
#include <limits>
#include <cassert>

namespace ceddec {
    enum class Architecture : uint8_t {
        x86_64,
        x86_32
    };

    enum class Register : uint16_t {
        Unknown,
        /* 64-bit general purpose */
        RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP,
        R8,  R9,  R10, R11, R12, R13, R14, R15,
        /* 32-bit general purpose */
        EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP,
        R8D, R9D, R10D, R11D, R12D, R13D, R14D, R15D,
        /* 16-bit general purpose */
        AX,  BX,  CX,  DX,  SI,  DI,  BP,  SP,
        R8W, R9W, R10W, R11W, R12W, R13W, R14W, R15W,
        /* 8-bit general purpose */
        AL,  BL,  CL,  DL,  SIL, DIL, BPL, SPL,
        AH,  BH,  CH,  DH,
        R8B, R9B, R10B, R11B, R12B, R13B, R14B, R15B,
        /* instruction pointer */
        RIP, EIP, IP,
        /* floating point (x87) */
        ST0, ST1, ST2, ST3, ST4, ST5, ST6, ST7,
        /* mmx */
        MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
        /* 128-bit vector */
        XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
        XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
        XMM16, XMM17, XMM18, XMM19, XMM20, XMM21, XMM22, XMM23,
        XMM24, XMM25, XMM26, XMM27, XMM28, XMM29, XMM30, XMM31,
        /* 256-bit vector */
        YMM0, YMM1, YMM2, YMM3, YMM4, YMM5, YMM6, YMM7,
        YMM8, YMM9, YMM10, YMM11, YMM12, YMM13, YMM14, YMM15,
        YMM16, YMM17, YMM18, YMM19, YMM20, YMM21, YMM22, YMM23,
        YMM24, YMM25, YMM26, YMM27, YMM28, YMM29, YMM30, YMM31,
        /* 512-bit vector */
        ZMM0, ZMM1, ZMM2, ZMM3, ZMM4, ZMM5, ZMM6, ZMM7,
        ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM14, ZMM15,
        ZMM16, ZMM17, ZMM18, ZMM19, ZMM20, ZMM21, ZMM22, ZMM23,
        ZMM24, ZMM25, ZMM26, ZMM27, ZMM28, ZMM29, ZMM30, ZMM31,
        /* mask registers */
        K0, K1, K2, K3, K4, K5, K6, K7,
        /* segment registers */
        CS, DS, ES, FS, GS, SS,
        /* control / debug / status registers */
        CR0, CR1, CR2, CR3, CR4, CR5, CR6, CR7, CR8, CR9, CR10, CR11, CR12, CR13, CR14, CR15,
        DR0, DR1, DR2, DR3, DR4, DR5, DR6, DR7, DR8, DR9, DR10, DR11, DR12, DR13, DR14, DR15,
        RFLAGS
    };

    /*
     * x86 condition codes, shared by Jcc / SETcc / CMOVcc / LOOPcc so the lifter can carry
     * the actual predicate through instead of collapsing them all into one opaque opcode
     */
    enum class ConditionCode : uint8_t {
        None,
        O,  NO,   /* overflow / not overflow */
        B,  AE,   /* below / above-or-equal   (unsigned, CF) */
        E,  NE,   /* equal / not equal        (ZF) */
        BE, A,    /* below-or-equal / above   (unsigned) */
        S,  NS,   /* sign / not sign */
        P,  NP,   /* parity / not parity */
        L,  GE,   /* less / greater-or-equal  (signed) */
        LE, G     /* less-or-equal / greater  (signed) */
    };

    /* string-op repeat prefixes */
    enum class RepPrefix : uint8_t {
        None,
        Rep,     /* rep / repe / repz */
        Repne    /* repne / repnz */
    };

    enum class OperandType : uint8_t {
        Register,
        Memory,
        Immediate,
        Label,
        Unknown
    };

    enum class SegmentReg : uint8_t {
        None,
        Fs,
        Gs,
        Cs,
        Ds,
        Es,
        Ss
    };

    struct CEDDEC_API MemoryOperand {
        std::string_view base;
        std::string_view index;
        uint8_t scale = 1;
        int64_t displacement = 0;
        bool rip_relative = false;
    };

    struct CEDDEC_API ParsedOperand {
        std::string_view raw_text;
        OperandType type = OperandType::Unknown;
        SegmentReg segment = SegmentReg::None;
        uint16_t bit_width = 0;
        MemoryOperand mem;
        int64_t immediate_val = 0;
    };

    struct CEDDEC_API ParsedInstruction {
        std::string_view mnemonic;
        std::vector<ParsedOperand> operands;
        ConditionCode condition = ConditionCode::None; /* set for jcc / setcc / cmovcc / loopcc */
        RepPrefix rep = RepPrefix::None;
        bool lock = false;
        bool is_valid = false;
    };

    struct CEDDEC_API ParsedBlock {
        std::vector<ParsedInstruction> instructions;
    };

    enum class IROpcode : uint16_t {
        Unknown,
        Nop,
        Phi,
        /* data transfer */
        Assign, MoveConditional, Exchange, CompareExchange,
        /* integer arithmetic */
        Add, AddCarry, Sub, SubBorrow, Mul, Div, Inc, Dec, Neg,
        /* logical / bitwise */
        And, Or, Xor, Not, Shl, Shr, Rol, Ror,
        /* bit manipulation */
        BitTest, BitSet, BitReset, BitComplement, BitScanFwd, BitScanRev, PopCount,
        /* control flow */
        Compare, Test, ConditionalSet, Jump, JumpConditional, Call, Return,
        /* string operations */
        StringMove, StringCompare, StringStore, StringLoad, StringScan,
        /* floating point (x87) */
        FloatAdd, FloatSub, FloatMul, FloatDiv, FloatSqrt, FloatCompare,
        /* simd / vector */
        VectorAssign, VectorAdd, VectorSub, VectorMul, VectorDiv, VectorLogical,
        VectorCompare, VectorShuffle, VectorInsert, VectorExtract,
        /* synchronization & system */
        SystemSync, SystemPrivileged, Syscall, Interrupt,
        /* virtualization & crypto */
        Virtualization, Cryptography,
        /* stack operations */
        Push, Pop
    };

    struct CEDDEC_API IROperand {
        std::variant<Register, MemoryOperand, int64_t, std::string> value;
        uint32_t ssa_version = 0;
    };

    struct CEDDEC_API IRInstruction {
        IROpcode opcode = IROpcode::Unknown;
        ConditionCode condition = ConditionCode::None; /* predicate for JumpConditional / ConditionalSet / MoveConditional */
        IROperand destination;
        IROperand source;
        bool is_valid = false;
        std::vector<IROperand> phi_sources;
        size_t original_index = 0;
    };

    struct CEDDEC_API BasicBlock {
        size_t id = 0;
        std::string label;
        std::vector<IRInstruction> instructions;
        std::vector<size_t> successors;

        /* stuff for Global SSA and Dominator calculations */
        std::vector<size_t> predecessors;
        std::vector<size_t> dominators;
        size_t idom = static_cast<size_t>(-1);
        std::vector<size_t> dom_frontier;
    };

    struct CEDDEC_API ControlFlowGraph {
        std::vector<BasicBlock> blocks;
    };

    struct CEDDEC_API SSARenameState {
        std::unordered_map<Register, std::vector<uint32_t>> stacks;
        std::unordered_map<Register, uint32_t> counters;
    };

    struct FunctionSignature {
        std::string name = "sub_function";
        std::string return_type = "int64_t";
        std::vector<std::string> params;
    };
}
