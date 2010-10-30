#readelf: -wf
#name: CFI on hppa
The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 4
  Data alignment factor: -[48]
  Return address column: 2
  Augmentation data:     1b

  DW_CFA_def_cfa: r30 ofs 0

00000014 00000018 00000018 FDE cie=00000000 pc=00000000..00000018
  DW_CFA_advance_loc: 8 to 00000008
  DW_CFA_def_cfa_reg: r3
  DW_CFA_advance_loc: 4 to 0000000c
  DW_CFA_def_cfa_offset: 4660
  DW_CFA_advance_loc: 8 to 00000014
  DW_CFA_def_cfa_reg: r30
  DW_CFA_nop

00000030 00000018 00000034 FDE cie=00000000 pc=00000018..00000040
  DW_CFA_advance_loc: 12 to 00000024
  DW_CFA_def_cfa_reg: r3
  DW_CFA_offset: r2 at cfa-24
  DW_CFA_advance_loc: 24 to 0000003c
  DW_CFA_def_cfa_reg: r30
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000004c 00000010 00000050 FDE cie=00000000 pc=00000040..00000048
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

