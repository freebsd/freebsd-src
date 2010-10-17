% { dg-do assemble { target mmix-*-* } }

# Base-plus-offset without -linker-allocated-gregs.  Note the constant.

a IS 42
b IS 112
  LDO $43,a+52		% { dg-error "no suitable GREG definition" "" }
  LDA $47,a+112		% { dg-error "no suitable GREG definition" "" }
  LDA $48,b+22		% { dg-error "no suitable GREG definition" "" }
  LDO $43,c+2		% { dg-error "no suitable GREG definition" "" }
  LDA $47,d+212		% { dg-error "no suitable GREG definition" "" }
  LDA $48,c+21		% { dg-error "no suitable GREG definition" "" }
c IS 72
d IS 3
