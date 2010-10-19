#readelf: -wf
#name: CFI on s390x
#as: -m64 -march=z900

The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: -8
  Return address column: 14
  Augmentation data:     1b

  DW_CFA_def_cfa: r15 ofs 160
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000018 00000024 0000001c FDE cie=00000000 pc=00000000..00000070
  DW_CFA_advance_loc: 6 to 00000006
  DW_CFA_offset: r15 at cfa-40
  DW_CFA_offset: r14 at cfa-48
  DW_CFA_offset: r13 at cfa-56
  DW_CFA_offset: r12 at cfa-64
  DW_CFA_offset: r11 at cfa-72
  DW_CFA_offset: r10 at cfa-80
  DW_CFA_offset: r9 at cfa-88
  DW_CFA_offset: r8 at cfa-96
  DW_CFA_advance_loc: 8 to 0000000e
  DW_CFA_def_cfa_offset: 320
  DW_CFA_nop
  DW_CFA_nop

