// SPDX-License-Identifier: GPL-2.0-or-later
// gcn_dis: lists a dumped GCN shader (shadPS4 shader/dumps/*.bin) with the port's own decoder,
// so the listing matches what the recompiler sees. Usage: gcn_dis <shader.bin>
#include <cstdio>
#include <vector>
#include <magic_enum/magic_enum.hpp>
#include "shader_recompiler/frontend/decode.h"

using namespace Shader::Gcn;

static void operand(const InstOperand& op) {
    const auto field = magic_enum::enum_name(op.field);
    switch (op.field) {
    case OperandField::ScalarGPR: std::printf(" s%u", op.code); break;
    case OperandField::VectorGPR: std::printf(" v%u", op.code); break;
    case OperandField::LiteralConst: std::printf(" lit:0x%x", op.code); break;
    case OperandField::SignedConstIntPos:
    case OperandField::SignedConstIntNeg:
    case OperandField::ConstFloatPos_0_5: case OperandField::ConstFloatNeg_0_5:
    case OperandField::ConstFloatPos_1_0: case OperandField::ConstFloatNeg_1_0:
    case OperandField::ConstFloatPos_2_0: case OperandField::ConstFloatNeg_2_0:
    case OperandField::ConstFloatPos_4_0: case OperandField::ConstFloatNeg_4_0:
        std::printf(" %.*s(0x%x)", int(field.size()), field.data(), op.code); break;
    default: std::printf(" %.*s", int(field.size()), field.data()); break;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: gcn_dis <shader.bin>\n"); return 2; }
    std::FILE* f = std::fopen(argv[1], "rb");
    if (!f) { std::perror(argv[1]); return 1; }
    std::vector<u32> words;
    u32 w;
    while (std::fread(&w, 4, 1, f) == 1) words.push_back(w);
    std::fclose(f);
    GcnCodeSlice code(words.data(), words.data() + words.size());
    GcnDecodeContext decoder;
    u32 pc = 0;
    while (!code.atEnd()) {
        const GcnInst inst = decoder.decodeInstruction(code);
        const auto name = magic_enum::enum_name(inst.opcode);
        std::printf("%04x  %-28.*s", pc, int(name.size()), name.data());
        for (u32 i = 0; i < inst.dst_count; ++i) operand(inst.dst[i]);
        std::printf(" <-");
        for (u32 i = 0; i < inst.src_count; ++i) operand(inst.src[i]);
        if (inst.encoding == InstEncoding::DS)
            std::printf("  [off0=%u off1=%u]", unsigned(inst.control.ds.offset0), unsigned(inst.control.ds.offset1));
        if (inst.encoding == InstEncoding::SOPP)
            std::printf("  [simm=0x%x]", unsigned(inst.control.sopp.simm));
        std::printf("\n");
        pc += inst.length;
        if (inst.opcode == Opcode::S_ENDPGM) break;
    }
    return 0;
}
