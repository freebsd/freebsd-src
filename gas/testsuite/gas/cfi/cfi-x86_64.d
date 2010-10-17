#readelf: -wf
#name: CFI on x86-64
The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: -8
  Return address column: 16
  Augmentation data:     1b

  DW_CFA_def_cfa: r7 ofs 8
  DW_CFA_offset: r16 at cfa-8
  DW_CFA_nop
  DW_CFA_nop

00000018 00000014 0000001c FDE cie=00000000 pc=00000020..00000034
  DW_CFA_advance_loc: 7 to 00000027
  DW_CFA_def_cfa_offset: 4668
  DW_CFA_advance_loc: 12 to 00000033
  DW_CFA_def_cfa_offset: 8

00000030 0000001c 00000034 FDE cie=00000000 pc=00000038..00000047
  DW_CFA_advance_loc: 1 to 00000039
  DW_CFA_def_cfa_offset: 16
  DW_CFA_offset: r6 at cfa-16
  DW_CFA_advance_loc: 3 to 0000003c
  DW_CFA_def_cfa_reg: r6
  DW_CFA_advance_loc: 10 to 00000046
  DW_CFA_def_cfa: r7 ofs 8
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000050 00000014 00000054 FDE cie=00000000 pc=00000058..0000006b
  DW_CFA_advance_loc: 3 to 0000005b
  DW_CFA_def_cfa_reg: r12
  DW_CFA_advance_loc: 15 to 0000006a
  DW_CFA_def_cfa_reg: r7
  DW_CFA_nop

00000068 00000010 0000006c FDE cie=00000000 pc=00000070..00000076
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000007c 00000010 00000080 FDE cie=00000000 pc=00000084..00000096
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

