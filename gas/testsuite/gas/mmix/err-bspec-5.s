% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
here SWYM 0,0,0
 BSPEC 0
 TETRA 4
 ESPEC
 BSPEC 65535
 TETRA 4
 ESPEC
 BSPEC 65536 % { dg-error "invalid BSPEC expression" "" }
 TETRA 4
 ESPEC
 BSPEC forw % { dg-error "invalid BSPEC expression" "" }
 TETRA 4
 ESPEC
 BSPEC here % { dg-error "invalid BSPEC expression" "" }
 TETRA 4
 ESPEC
 BSPEC -1 % { dg-error "invalid BSPEC expression" "" }
 TETRA 4
 ESPEC
