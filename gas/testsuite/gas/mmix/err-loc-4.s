% { dg-do assemble { target mmix-*-* } }
 LOC #201
 WYDE 1
 SWYM 1 % { dg-error "specified location wasn't TETRA-aligned" "" }
