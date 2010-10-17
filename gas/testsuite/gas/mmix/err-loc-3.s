% { dg-do assemble { target mmix-*-* } }
 LOC (#20 << 56) + #202
 TETRA 1
 OCTA 1 % { dg-error "data item with alignment larger than location" "" }
