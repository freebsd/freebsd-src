% { dg-do assemble { target mmix-*-* } }
 LOC #201
 .p2align 0
 TETRA 1 % { dg-error "unaligned data at an absolute location" "" }
