#readelf: -wf
#name: CFI on i386
The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: -4
  Return address column: 8
  Augmentation data:     1b

  DW_CFA_def_cfa: r4 ofs 4
  DW_CFA_offset: r8 at cfa-4
  DW_CFA_nop
  DW_CFA_nop

00000018 00000014 0000001c FDE cie=00000000 pc=00000000..00000012
  DW_CFA_advance_loc: 6 to 00000006
  DW_CFA_def_cfa_offset: 4664
  DW_CFA_advance_loc: 11 to 00000011
  DW_CFA_def_cfa_offset: 4

00000030 00000018 00000034 FDE cie=00000000 pc=00000012..0000001f
  DW_CFA_advance_loc: 1 to 00000013
  DW_CFA_def_cfa_offset: 8
  DW_CFA_offset: r5 at cfa-8
  DW_CFA_advance_loc: 2 to 00000015
  DW_CFA_def_cfa_reg: r5
  DW_CFA_advance_loc: 9 to 0000001e
  DW_CFA_def_cfa_reg: r4

0000004c 00000014 00000050 FDE cie=00000000 pc=0000001f..0000002f
  DW_CFA_advance_loc: 2 to 00000021
  DW_CFA_def_cfa_reg: r3
  DW_CFA_advance_loc: 13 to 0000002e
  DW_CFA_def_cfa: r4 ofs 4

00000064 00000010 00000068 FDE cie=00000000 pc=0000002f..00000035
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000078 00000010 0000007c FDE cie=00000000 pc=00000035..00000044
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

