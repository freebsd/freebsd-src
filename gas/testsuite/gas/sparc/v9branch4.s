   # Text for relocation overflow diagnostic
   .text
   bne,pt %icc, 1f
   nop
   .skip (1024 * 1024 - 12)
   nop
1: nop
