#source: eh1.s
#source: eh2a.s
#ld:
#readelf: -wf
#target: x86_64-*-*

The section .eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          ""
  Code alignment factor: 1
  Data alignment factor: -8
  Return address column: 16

  DW_CFA_def_cfa: r7 ofs 8
  DW_CFA_offset: r16 at cfa-8
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000018 0000001c 0000001c FDE cie=00000000 pc=004000b0..004000b0
  DW_CFA_advance_loc: 0 to 004000b0
  DW_CFA_def_cfa_offset: 16
  DW_CFA_offset: r6 at cfa-16
  DW_CFA_advance_loc: 0 to 004000b0
  DW_CFA_def_cfa_reg: r6

00000038 ZERO terminator

