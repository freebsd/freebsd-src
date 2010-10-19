#readelf: -wf
#name: CFI on mips, 1
The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: -4
  Return address column: 31
  Augmentation data:     0b

  DW_CFA_def_cfa_reg: r29
  DW_CFA_def_cfa: r29 ofs 0
  DW_CFA_nop
  DW_CFA_nop

00000018 0000001c 0000001c FDE cie=00000000 pc=00000000..0000002c
  DW_CFA_advance_loc: 4 to 00000004
  DW_CFA_def_cfa_offset: 8
  DW_CFA_advance_loc: 4 to 00000008
  DW_CFA_offset: r30 at cfa-8
  DW_CFA_advance_loc: 4 to 0000000c
  DW_CFA_def_cfa: r30 ofs 8
  DW_CFA_advance_loc: 24 to 00000024
  DW_CFA_def_cfa: r29 ofs 0
  DW_CFA_nop
