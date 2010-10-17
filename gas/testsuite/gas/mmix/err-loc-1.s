% { dg-do assemble { target mmix-*-* } }
 LOC #200
Main SET $45,23
 LOC #100 % { dg-error "LOC expression stepping backwards" "" }
 SET $57,$67
