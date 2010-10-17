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

00000018 00000014 0000001c FDE cie=00000000 pc=00000020..00000032
  DW_CFA_advance_loc: 6 to 00000026
  DW_CFA_def_cfa_offset: 4664
  DW_CFA_advance_loc: 11 to 00000031
  DW_CFA_def_cfa_offset: 4

00000030 00000018 00000034 FDE cie=00000000 pc=0000004a..00000057
  DW_CFA_advance_loc: 1 to 0000004b
  DW_CFA_def_cfa_offset: 8
  DW_CFA_offset: r5 at cfa-8
  DW_CFA_advance_loc: 2 to 0000004d
  DW_CFA_def_cfa_reg: r5
  DW_CFA_advance_loc: 9 to 00000056
  DW_CFA_def_cfa_reg: r4

0000004c 00000014 00000050 FDE cie=00000000 pc=00000073..00000083
  DW_CFA_advance_loc: 2 to 00000075
  DW_CFA_def_cfa_reg: r3
  DW_CFA_advance_loc: 13 to 00000082
  DW_CFA_def_cfa: r4 ofs 4

00000064 00000010 00000068 FDE cie=00000000 pc=0000009b..000000a1
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000078 00000010 0000007c FDE cie=00000000 pc=000000b5..000000c4
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

