#readelf: -wf
#name: CFI on m68k
The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 2
  Data alignment factor: -4
  Return address column: 24
  Augmentation data:     1b

  DW_CFA_def_cfa: r15 ofs 4
  DW_CFA_offset: r24 at cfa-4
  DW_CFA_nop
  DW_CFA_nop

00000018 00000014 0000001c FDE cie=00000000 pc=00000000..0000000c
  DW_CFA_advance_loc: 4 to 00000004
  DW_CFA_def_cfa_offset: 4664
  DW_CFA_advance_loc: 6 to 0000000a
  DW_CFA_def_cfa_offset: 4

00000030 00000018 00000034 FDE cie=00000000 pc=0000000c..00000018
  DW_CFA_advance_loc: 4 to 00000010
  DW_CFA_def_cfa_offset: 8
  DW_CFA_offset: r14 at cfa-8
  DW_CFA_def_cfa_reg: r14
  DW_CFA_advance_loc: 6 to 00000016
  DW_CFA_def_cfa_reg: r15
  DW_CFA_nop

0000004c 00000010 00000050 FDE cie=00000000 pc=00000018..0000001c

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

