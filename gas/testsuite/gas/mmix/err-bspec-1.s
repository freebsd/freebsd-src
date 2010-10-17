% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
 BSPEC 5 % { dg-error "BSPEC without ESPEC" "" }
 TETRA 4
