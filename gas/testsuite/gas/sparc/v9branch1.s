   # Check if maximum possible branch distances for v9 branches are accepted
   .text
   brz,pt %o0, 1f
   nop
   .skip (128 * 1024 - 16)
   nop
1: nop
   .skip (128 * 1024 - 4)
   brz,pt %o0, 1b
   nop
   bne,pt %icc, 2f
   nop
   .skip (1024 * 1024 - 16)
   nop
2: nop
   .skip (1024 * 1024 - 4)
   bne,pt %icc, 2b
   nop
