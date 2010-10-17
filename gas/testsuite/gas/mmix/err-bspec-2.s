% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
 BSPEC 5
 TETRA 4
 BSPEC 6 % { dg-error "BSPEC already active" "" }
 TETRA 5
 ESPEC
