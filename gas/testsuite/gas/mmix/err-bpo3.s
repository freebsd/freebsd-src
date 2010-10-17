% { dg-do assemble { target mmix-*-* } }

# Base-plus-offset without -linker-allocated-gregs.

a TETRA 42
  LDO $43,a+52		% { dg-error "no suitable GREG definition" "" }

  LOC @+256
d TETRA 28
  LDO $143,d+12		% { dg-error "no suitable GREG definition" "" }
  LDO $243,a+12		% { dg-error "no suitable GREG definition" "" }
  LDA $103,d+40		% { dg-error "no suitable GREG definition" "" }
  LDA $13,a+24		% { dg-error "no suitable GREG definition" "" }
