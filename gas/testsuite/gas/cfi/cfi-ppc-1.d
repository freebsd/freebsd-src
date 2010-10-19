#readelf: -wf
#name: CFI on ppc
#as: -a32

The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 4
  Data alignment factor: -4
  Return address column: 65
  Augmentation data:     1b

  DW_CFA_def_cfa: r1 ofs 0

00000014 00000020 00000018 FDE cie=00000000 pc=00000000..00000070
  DW_CFA_advance_loc: 4 to 00000004
  DW_CFA_def_cfa_offset: 48
  DW_CFA_advance_loc: 16 to 00000014
  DW_CFA_offset: r27 at cfa-20
  DW_CFA_offset: r26 at cfa-24
  DW_CFA_offset_extended_sf: r65 at cfa\+4
  DW_CFA_advance_loc: 8 to 0000001c
  DW_CFA_offset: r28 at cfa-16
  DW_CFA_advance_loc: 8 to 00000024
  DW_CFA_offset: r29 at cfa-12
  DW_CFA_nop
  DW_CFA_nop

