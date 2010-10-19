; Check that byte- and word-size special registers on CRISv32
; take 32-bit immediate operands, as opposed to pre-v32 CRIS.

 .text
here:
 move 0xfefdfcfa,$vr
 move 0xf00fba11,$pid
 move extsym,$srs
 move extsym2,$wz
 move 1001,$exs
 move 101,$eda
