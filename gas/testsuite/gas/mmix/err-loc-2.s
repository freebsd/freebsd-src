% { dg-do assemble { target mmix-*-* } }
 LOC (#20 << 56) + #200
 TETRA 1
 LOC (#20 << 56) + #100 % { dg-error "LOC expression stepping backwards" "" }
 TETRA 2
