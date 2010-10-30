   # Text for relocation overflow diagnostic
   .text
1: nop
   .skip (1024 * 1024)
   bne,pt %icc, 1b
   nop
