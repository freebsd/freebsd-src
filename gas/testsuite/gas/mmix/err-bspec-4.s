% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
 BSPEC 2
 TETRA 4
 ESPEC
 TETRA 5
 ESPEC % { dg-error "ESPEC without preceding BSPEC" "" }
