struct ia64_opcode ia64_opcodes_d[] =
  {
    {"add",   IA64_TYPE_DYN, 1, 0, 0,
     {IA64_OPND_R1, IA64_OPND_IMM22, IA64_OPND_R3_2}},
    {"add",   IA64_TYPE_DYN, 1, 0, 0,
     {IA64_OPND_R1, IA64_OPND_IMM14, IA64_OPND_R3}},
    {"break", IA64_TYPE_DYN, 0, 0, 0, {IA64_OPND_IMMU21}},
    {"chk.s", IA64_TYPE_DYN, 0, 0, 0, {IA64_OPND_R2, IA64_OPND_TGT25b}},
    {"mov",   IA64_TYPE_DYN, 1, 0, 0, {IA64_OPND_R1,  IA64_OPND_AR3}},
    {"mov",   IA64_TYPE_DYN, 1, 0, 0, {IA64_OPND_AR3, IA64_OPND_IMM8}},
    {"mov",   IA64_TYPE_DYN, 1, 0, 0, {IA64_OPND_AR3, IA64_OPND_R2}},
    {"nop",   IA64_TYPE_DYN, 0, 0, 0, {IA64_OPND_IMMU21}},
    {0}
  };
