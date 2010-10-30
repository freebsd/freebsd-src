   # Text for relocation overflow diagnostic
   .text
   brz,pt %o0, 1f
   nop
   .skip (128 * 1024 - 12)
   nop
1: nop
