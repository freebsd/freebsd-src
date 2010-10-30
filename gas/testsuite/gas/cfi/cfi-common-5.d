#readelf: -wf
#name: CFI common 5
The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     [01]b
#...
00000014 00000014 00000018 FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_remember_state
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_restore_state
#...
0000002c 0000001[48] 00000030 FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa_offset: 0
#pass
