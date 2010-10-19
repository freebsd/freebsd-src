#readelf: -wf
#name: CFI common 4
The section .eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     [01]b
#...
00000014 00000010 00000018 FDE cie=00000000 pc=.*
  DW_CFA_remember_state
  DW_CFA_restore_state
#...
00000028 0000001[04] 0000002c FDE cie=00000000 pc=.*
  DW_CFA_remember_state
  DW_CFA_restore_state
#pass
