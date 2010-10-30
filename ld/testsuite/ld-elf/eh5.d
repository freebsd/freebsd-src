#source: eh5.s
#source: eh5a.s
#source: eh5b.s
#ld:
#readelf: -wf
#target: x86_64-*-* i?86-*-*

The section .eh_frame contains:

00000000 0000001[04] 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     1b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
#...
0000001[48] 00000014 0000001[8c] FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000(2c|30) 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     03 .. .. .. .. 1b

  DW_CFA_nop

0000004[48] 00000014 0000001c FDE cie=000000(2c|30) pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000(5c|60) 00000014 0000006[04] FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000007[48] 0000001[8c] 00000000 CIE
  Version:               1
  Augmentation:          "zPLR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     03 .. .. .. .. 0c 1b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
#...
0000009[08] 0000001c 0000002[04] FDE cie=0000007[48] pc=.*
  Augmentation data:     (ef be ad de 00 00 00 00|00 00 00 00 de ad be ef)

  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000b[08] 0000001[04] 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     1b

  DW_CFA_def_cfa: r0 ofs 16
#...
000000(c4|d0) 0000001[04] 0000001[8c] FDE cie=000000b[08] pc=.*
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
#...
000000[de]8 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     03 .. .. .. .. 1b

  DW_CFA_nop

00000(0f|10)0 00000014 0000001c FDE cie=000000[de]8 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001[01]8 0000001[04] 000000(5c|64) FDE cie=000000b[08] pc=.*
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
#...
000001(1c|30) 0000001[8c] 00000000 CIE
  Version:               1
  Augmentation:          "zPLR"
  Code alignment factor: 1
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     03 .. .. .. .. 0c 1b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
#...
000001(38|50) 0000001c 0000002[04] FDE cie=000001(1c|30) pc=.*
  Augmentation data:     (ef be ad de 00 00 00 00|00 00 00 00 de ad be ef)

  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001(58|70) 00000014 000001(5c|74) FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001(70|88) 00000014 000001(48|5c) FDE cie=000000(2c|30) pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001(88|a0) 00000014 000001(8c|a4) FDE cie=00000000 pc=.*
  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001(a0|b8) 0000001c 000001(30|44) FDE cie=0000007[48] pc=.*
  Augmentation data:     (ef be ad de 00 00 00 00|00 00 00 00 de ad be ef)

  DW_CFA_advance_loc: 4 to .*
  DW_CFA_def_cfa: r0 ofs 16
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

