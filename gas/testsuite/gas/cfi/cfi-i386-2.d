#readelf: -wf
#name: CFI on i386, 2
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

00000018 00000018 0000001c FDE cie=00000000 pc=00000020..00000029
  DW_CFA_advance_loc: 1 to 00000021
  DW_CFA_def_cfa_offset: 8
  DW_CFA_offset: r5 at cfa-8
  DW_CFA_advance_loc: 4 to 00000025
  DW_CFA_offset: r3 at cfa-12
  DW_CFA_def_cfa_offset: 12
  DW_CFA_nop

