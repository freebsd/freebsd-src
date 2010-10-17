% { dg-do assemble { target mmix-*-* } }
Main SWYM 0,0,0
 FLOT $112,$223,$41 % { dg-error "invalid operands" "Y field of FLOT 1" }
 FLOT $112,$223,141 % { dg-error "invalid operands" "Y field of FLOT 2" }
 LDA $122,$203,256 % { dg-error "invalid operands" "Z field too large" }
