#readelf: -wf
#name: CFI on alpha, 3
The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 4
  Data alignment factor: -8
  Return address column: 26
  Augmentation data:     1b

  DW_CFA_def_cfa_reg: r30
  DW_CFA_nop

00000014 00000028 00000018 FDE cie=00000000 pc=00000000..00000040
  DW_CFA_advance_loc: 4 to 00000004
  DW_CFA_def_cfa_offset: 32
  DW_CFA_advance_loc: 4 to 00000008
  DW_CFA_offset: r26 at cfa-32
  DW_CFA_advance_loc: 4 to 0000000c
  DW_CFA_offset: r9 at cfa-24
  DW_CFA_advance_loc: 4 to 00000010
  DW_CFA_offset: r15 at cfa-16
  DW_CFA_advance_loc: 4 to 00000014
  DW_CFA_offset: r34 at cfa-8
  DW_CFA_advance_loc: 4 to 00000018
  DW_CFA_def_cfa_reg: r15
  DW_CFA_advance_loc: 36 to 0000003c
  DW_CFA_def_cfa: r30 ofs 0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

