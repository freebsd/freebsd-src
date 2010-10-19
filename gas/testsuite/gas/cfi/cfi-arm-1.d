#readelf: -wf
#name: CFI on ARM

The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 2
  Data alignment factor: -4
  Return address column: 14
  Augmentation data:     1b

  DW_CFA_def_cfa: r13 ofs 0

00000014 00000020 00000018 FDE cie=00000000 pc=00000000..00000018
  DW_CFA_advance_loc: 4 to 00000004
  DW_CFA_def_cfa: r12 ofs 0
  DW_CFA_advance_loc: 4 to 00000008
  DW_CFA_def_cfa: r13 ofs 16
  DW_CFA_advance_loc: 4 to 0000000c
  DW_CFA_def_cfa_offset: 32
  DW_CFA_offset: r11 at cfa-32
  DW_CFA_offset: r14 at cfa-24
  DW_CFA_advance_loc: 4 to 00000010
  DW_CFA_def_cfa: r11 ofs 16

