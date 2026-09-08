#pragma once
#include <ceddec/types.hpp>

[[gnu::hot]] static inline ceddec::IROpcode MapOpcode(std::string_view m) {
    static const std::unordered_map<std::string_view, ceddec::IROpcode> instruction_map = {
        /* data transfer */
        {"mov", ceddec::IROpcode::Assign}, {"lea", ceddec::IROpcode::Assign},
        {"movzx", ceddec::IROpcode::Assign}, {"movsx", ceddec::IROpcode::Assign},
        {"movsxd", ceddec::IROpcode::Assign}, {"cdqe", ceddec::IROpcode::Assign},
        {"cdq", ceddec::IROpcode::Assign}, {"cqo", ceddec::IROpcode::Assign},
        {"cbw", ceddec::IROpcode::Assign}, {"cwde", ceddec::IROpcode::Assign},
        {"xchg", ceddec::IROpcode::Exchange}, {"cmpxchg", ceddec::IROpcode::CompareExchange},
        {"bswap", ceddec::IROpcode::Assign},

        /* arithmetic */
        {"add", ceddec::IROpcode::Add}, {"sub", ceddec::IROpcode::Sub},
        {"adc", ceddec::IROpcode::AddCarry}, {"sbb", ceddec::IROpcode::SubBorrow},
        {"mul", ceddec::IROpcode::Mul}, {"imul", ceddec::IROpcode::Mul},
        {"div", ceddec::IROpcode::Div}, {"idiv", ceddec::IROpcode::Div},
        {"inc", ceddec::IROpcode::Inc}, {"dec", ceddec::IROpcode::Dec},
        {"neg", ceddec::IROpcode::Neg}, {"xadd", ceddec::IROpcode::Exchange},

        /* logic / bitwise */
        {"and", ceddec::IROpcode::And}, {"or", ceddec::IROpcode::Or},
        {"xor", ceddec::IROpcode::Xor}, {"not", ceddec::IROpcode::Not},
        {"shl", ceddec::IROpcode::Shl}, {"shr", ceddec::IROpcode::Shr},
        {"sal", ceddec::IROpcode::Shl}, {"sar", ceddec::IROpcode::Shr},
        {"rol", ceddec::IROpcode::Rol}, {"ror", ceddec::IROpcode::Ror},
        {"rcl", ceddec::IROpcode::Rol}, {"rcr", ceddec::IROpcode::Ror},

        /* bit manipulation */
        {"bt", ceddec::IROpcode::BitTest}, {"bts", ceddec::IROpcode::BitSet},
        {"btr", ceddec::IROpcode::BitReset}, {"btc", ceddec::IROpcode::BitComplement},
        {"bsf", ceddec::IROpcode::BitScanFwd}, {"tzcnt", ceddec::IROpcode::BitScanFwd},
        {"bsr", ceddec::IROpcode::BitScanRev}, {"lzcnt", ceddec::IROpcode::BitScanRev},
        {"popcnt", ceddec::IROpcode::PopCount},

        /* control flow */
        {"cmp", ceddec::IROpcode::Compare}, {"test", ceddec::IROpcode::Test},
        {"jmp", ceddec::IROpcode::Jump}, {"call", ceddec::IROpcode::Call},
        {"ret", ceddec::IROpcode::Return}, {"retn", ceddec::IROpcode::Return},
        {"iret", ceddec::IROpcode::Return}, {"iretd", ceddec::IROpcode::Return},
        {"iretq", ceddec::IROpcode::Return},
        {"syscall", ceddec::IROpcode::Syscall}, {"sysenter", ceddec::IROpcode::Syscall},
        {"int", ceddec::IROpcode::Interrupt}, {"int3", ceddec::IROpcode::Interrupt},

        /* counted / short-circuit loops / conditional branches */
        {"loop", ceddec::IROpcode::JumpConditional}, {"loope", ceddec::IROpcode::JumpConditional},
        {"loopz", ceddec::IROpcode::JumpConditional}, {"loopne", ceddec::IROpcode::JumpConditional},
        {"loopnz", ceddec::IROpcode::JumpConditional},
        {"jcxz", ceddec::IROpcode::JumpConditional}, {"jecxz", ceddec::IROpcode::JumpConditional},
        {"jrcxz", ceddec::IROpcode::JumpConditional},

        /* no-ops / hints */
        {"nop", ceddec::IROpcode::Nop}, {"pause", ceddec::IROpcode::Nop},
        {"endbr64", ceddec::IROpcode::Nop}, {"endbr32", ceddec::IROpcode::Nop},
        {"ud2", ceddec::IROpcode::Nop}, {"wait", ceddec::IROpcode::Nop}, {"fwait", ceddec::IROpcode::Nop},

        /* misc flag / privileged control */
        {"hlt", ceddec::IROpcode::SystemPrivileged},
        {"cli", ceddec::IROpcode::SystemPrivileged}, {"sti", ceddec::IROpcode::SystemPrivileged},
        {"clc", ceddec::IROpcode::SystemPrivileged}, {"stc", ceddec::IROpcode::SystemPrivileged},
        {"cmc", ceddec::IROpcode::SystemPrivileged},
        {"lahf", ceddec::IROpcode::Assign}, {"sahf", ceddec::IROpcode::Assign},

        /* floating point (x87) */
        {"fadd", ceddec::IROpcode::FloatAdd}, {"faddp", ceddec::IROpcode::FloatAdd},
        {"fiadd", ceddec::IROpcode::FloatAdd},
        {"fsub", ceddec::IROpcode::FloatSub}, {"fsubp", ceddec::IROpcode::FloatSub},
        {"fisub", ceddec::IROpcode::FloatSub},
        {"fmul", ceddec::IROpcode::FloatMul}, {"fmulp", ceddec::IROpcode::FloatMul},
        {"fimul", ceddec::IROpcode::FloatMul},
        {"fdiv", ceddec::IROpcode::FloatDiv}, {"fdivp", ceddec::IROpcode::FloatDiv},
        {"fidiv", ceddec::IROpcode::FloatDiv},
        {"fsqrt", ceddec::IROpcode::FloatSqrt},
        {"fcom", ceddec::IROpcode::FloatCompare}, {"fcomp", ceddec::IROpcode::FloatCompare},
        {"fcompp", ceddec::IROpcode::FloatCompare}, {"fcomi", ceddec::IROpcode::FloatCompare},
        {"fcomip", ceddec::IROpcode::FloatCompare}, {"fucomi", ceddec::IROpcode::FloatCompare},
        {"fucomip", ceddec::IROpcode::FloatCompare},

        /* strings */
        {"movsb", ceddec::IROpcode::StringMove}, {"movsw", ceddec::IROpcode::StringMove},
        {"movsd", ceddec::IROpcode::StringMove}, {"movsq", ceddec::IROpcode::StringMove},
        {"cmpsb", ceddec::IROpcode::StringCompare}, {"cmpsw", ceddec::IROpcode::StringCompare},
        {"cmpsd", ceddec::IROpcode::StringCompare}, {"cmpsq", ceddec::IROpcode::StringCompare},
        {"scasb", ceddec::IROpcode::StringScan}, {"scasw", ceddec::IROpcode::StringScan},
        {"scasd", ceddec::IROpcode::StringScan}, {"scasq", ceddec::IROpcode::StringScan},
        {"stosb", ceddec::IROpcode::StringStore}, {"stosw", ceddec::IROpcode::StringStore},
        {"stosd", ceddec::IROpcode::StringStore}, {"stosq", ceddec::IROpcode::StringStore},
        {"lodsb", ceddec::IROpcode::StringLoad}, {"lodsw", ceddec::IROpcode::StringLoad},
        {"lodsd", ceddec::IROpcode::StringLoad}, {"lodsq", ceddec::IROpcode::StringLoad},

        /* simd / vector assign */
        {"movaps", ceddec::IROpcode::VectorAssign}, {"movups", ceddec::IROpcode::VectorAssign},
        {"movapd", ceddec::IROpcode::VectorAssign}, {"movupd", ceddec::IROpcode::VectorAssign},
        {"movdqa", ceddec::IROpcode::VectorAssign}, {"movdqu", ceddec::IROpcode::VectorAssign},
        {"vmovaps", ceddec::IROpcode::VectorAssign}, {"vmovups", ceddec::IROpcode::VectorAssign},
        {"vmovapd", ceddec::IROpcode::VectorAssign}, {"vmovupd", ceddec::IROpcode::VectorAssign},
        {"vmovdqa", ceddec::IROpcode::VectorAssign}, {"vmovdqu", ceddec::IROpcode::VectorAssign},
        {"movss", ceddec::IROpcode::VectorAssign}, {"movsd", ceddec::IROpcode::VectorAssign},
        {"vmovss", ceddec::IROpcode::VectorAssign}, {"vmovsd", ceddec::IROpcode::VectorAssign},

        /* simd / vector math */
        {"addps", ceddec::IROpcode::VectorAdd}, {"addpd", ceddec::IROpcode::VectorAdd},
        {"vaddps", ceddec::IROpcode::VectorAdd}, {"vaddpd", ceddec::IROpcode::VectorAdd},
        {"addss", ceddec::IROpcode::VectorAdd}, {"addsd", ceddec::IROpcode::VectorAdd},
        {"vaddss", ceddec::IROpcode::VectorAdd}, {"vaddsd", ceddec::IROpcode::VectorAdd},

        {"subps", ceddec::IROpcode::VectorSub}, {"subpd", ceddec::IROpcode::VectorSub},
        {"vsubps", ceddec::IROpcode::VectorSub}, {"vsubpd", ceddec::IROpcode::VectorSub},
        {"subss", ceddec::IROpcode::VectorSub}, {"subsd", ceddec::IROpcode::VectorSub},
        {"vsubss", ceddec::IROpcode::VectorSub}, {"vsubsd", ceddec::IROpcode::VectorSub},

        {"mulps", ceddec::IROpcode::VectorMul}, {"mulpd", ceddec::IROpcode::VectorMul},
        {"vmulps", ceddec::IROpcode::VectorMul}, {"vmulpd", ceddec::IROpcode::VectorMul},
        {"mulss", ceddec::IROpcode::VectorMul}, {"mulsd", ceddec::IROpcode::VectorMul},
        {"vmulss", ceddec::IROpcode::VectorMul}, {"vmulsd", ceddec::IROpcode::VectorMul},

        {"divps", ceddec::IROpcode::VectorDiv}, {"divpd", ceddec::IROpcode::VectorDiv},
        {"vdivps", ceddec::IROpcode::VectorDiv}, {"vdivpd", ceddec::IROpcode::VectorDiv},
        {"divss", ceddec::IROpcode::VectorDiv}, {"divsd", ceddec::IROpcode::VectorDiv},
        {"vdivss", ceddec::IROpcode::VectorDiv}, {"vdivsd", ceddec::IROpcode::VectorDiv},

        /* simd / vector logic */
        {"andps", ceddec::IROpcode::VectorLogical}, {"andpd", ceddec::IROpcode::VectorLogical},
        {"vandps", ceddec::IROpcode::VectorLogical}, {"vandpd", ceddec::IROpcode::VectorLogical},
        {"orps", ceddec::IROpcode::VectorLogical}, {"orpd", ceddec::IROpcode::VectorLogical},
        {"vorps", ceddec::IROpcode::VectorLogical}, {"vorpd", ceddec::IROpcode::VectorLogical},
        {"xorps", ceddec::IROpcode::VectorLogical}, {"xorpd", ceddec::IROpcode::VectorLogical},
        {"vxorps", ceddec::IROpcode::VectorLogical}, {"vxorpd", ceddec::IROpcode::VectorLogical},
        {"pxor", ceddec::IROpcode::VectorLogical}, {"vpxor", ceddec::IROpcode::VectorLogical},
        {"pand", ceddec::IROpcode::VectorLogical}, {"vpand", ceddec::IROpcode::VectorLogical},
        {"por", ceddec::IROpcode::VectorLogical}, {"vpor", ceddec::IROpcode::VectorLogical},

        /* simd / vector compare */
        {"ucomiss", ceddec::IROpcode::VectorCompare}, {"ucomisd", ceddec::IROpcode::VectorCompare},
        {"vucomiss", ceddec::IROpcode::VectorCompare}, {"vucomisd", ceddec::IROpcode::VectorCompare},
        {"comiss", ceddec::IROpcode::VectorCompare}, {"comisd", ceddec::IROpcode::VectorCompare},
        {"vcomiss", ceddec::IROpcode::VectorCompare}, {"vcomisd", ceddec::IROpcode::VectorCompare},

        /* simd / vector shuffle */
        {"shufps", ceddec::IROpcode::VectorShuffle}, {"shufpd", ceddec::IROpcode::VectorShuffle},
        {"vshufps", ceddec::IROpcode::VectorShuffle}, {"vshufpd", ceddec::IROpcode::VectorShuffle},
        {"pshufd", ceddec::IROpcode::VectorShuffle}, {"vpshufd", ceddec::IROpcode::VectorShuffle},
        {"pshufb", ceddec::IROpcode::VectorShuffle}, {"vpshufb", ceddec::IROpcode::VectorShuffle},

        /* simd / vector extract & insert */
        {"pextrb", ceddec::IROpcode::VectorExtract}, {"pextrw", ceddec::IROpcode::VectorExtract},
        {"pextrd", ceddec::IROpcode::VectorExtract}, {"pextrq", ceddec::IROpcode::VectorExtract},
        {"vpextrb", ceddec::IROpcode::VectorExtract}, {"vpextrw", ceddec::IROpcode::VectorExtract},
        {"vpextrd", ceddec::IROpcode::VectorExtract}, {"vpextrq", ceddec::IROpcode::VectorExtract},
        {"extractps", ceddec::IROpcode::VectorExtract}, {"vextractps", ceddec::IROpcode::VectorExtract},

        {"pinsrb", ceddec::IROpcode::VectorInsert}, {"pinsrw", ceddec::IROpcode::VectorInsert},
        {"pinsrd", ceddec::IROpcode::VectorInsert}, {"pinsrq", ceddec::IROpcode::VectorInsert},
        {"vpinsrb", ceddec::IROpcode::VectorInsert}, {"vpinsrw", ceddec::IROpcode::VectorInsert},
        {"vpinsrd", ceddec::IROpcode::VectorInsert}, {"vpinsrq", ceddec::IROpcode::VectorInsert},
        {"insertps", ceddec::IROpcode::VectorInsert}, {"vinsertps", ceddec::IROpcode::VectorInsert},

        /* synchronization & system */
        {"mfence", ceddec::IROpcode::SystemSync}, {"lfence", ceddec::IROpcode::SystemSync},
        {"sfence", ceddec::IROpcode::SystemSync}, {"cpuid", ceddec::IROpcode::SystemPrivileged},
        {"rdtsc", ceddec::IROpcode::SystemPrivileged}, {"rdtscp", ceddec::IROpcode::SystemPrivileged},
        {"xgetbv", ceddec::IROpcode::SystemPrivileged}, {"xsetbv", ceddec::IROpcode::SystemPrivileged},
        {"invlpg", ceddec::IROpcode::SystemPrivileged}, {"wbinvd", ceddec::IROpcode::SystemPrivileged},

        /* virtualization & crypto */
        {"vmread", ceddec::IROpcode::Virtualization}, {"vmwrite", ceddec::IROpcode::Virtualization},
        {"vmlaunch", ceddec::IROpcode::Virtualization}, {"vmresume", ceddec::IROpcode::Virtualization},
        {"vmptrld", ceddec::IROpcode::Virtualization}, {"vmptrst", ceddec::IROpcode::Virtualization},
        {"vmclear", ceddec::IROpcode::Virtualization}, {"vmxon", ceddec::IROpcode::Virtualization},
        {"aesenc", ceddec::IROpcode::Cryptography}, {"aesenclast", ceddec::IROpcode::Cryptography},
        {"aesdec", ceddec::IROpcode::Cryptography}, {"aesdeclast", ceddec::IROpcode::Cryptography},
        {"aesimc", ceddec::IROpcode::Cryptography}, {"aeskeygenassist", ceddec::IROpcode::Cryptography},

        /* stack operations */
        {"push", ceddec::IROpcode::Push},
        {"pop",  ceddec::IROpcode::Pop},
    };

    if (auto it = instruction_map.find(m); it != instruction_map.end()) {
        return it->second;
    }

    if (m.starts_with("cmov")) return ceddec::IROpcode::MoveConditional;
    if (m.starts_with("set")) return ceddec::IROpcode::ConditionalSet;
    if (m.starts_with("j")) return ceddec::IROpcode::JumpConditional;

    return ceddec::IROpcode::Unknown;
}
