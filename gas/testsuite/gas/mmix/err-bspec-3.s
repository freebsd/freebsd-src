% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
 TETRA 4
 ESPEC % { dg-error "ESPEC without preceding BSPEC" "" }
