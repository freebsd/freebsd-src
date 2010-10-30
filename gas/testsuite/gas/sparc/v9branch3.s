   # Text for relocation overflow diagnostic
   .text
1: nop
   .skip (128 * 1024)
   brz,pt %o0, 1b
   nop
