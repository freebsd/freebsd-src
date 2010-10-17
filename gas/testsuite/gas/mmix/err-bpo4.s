% { dg-do assemble { target mmix-*-* } }

# Base-plus-offset without -linker-allocated-gregs.

Main PUSHGO $42,fn		% { dg-error "no suitable GREG definition" "" }
 SWYM 0
extfn POP 0,0
