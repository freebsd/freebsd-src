#readelf: -wf
#name: CFI common 1
The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     [01]b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000014 00000018 00000018 FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_offset: r1 at cfa-8
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa_offset: 32
  DW_CFA_offset: r2 at cfa-24
