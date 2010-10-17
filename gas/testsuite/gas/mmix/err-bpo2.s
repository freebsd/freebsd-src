% { dg-do assemble { target mmix-*-* } }

# Check that base-plus-offset relocs without suitable GREGs are not passed
# through (without -linker-allocated-gregs).
a TETRA 42
  LDO $43,a+52		% { dg-error "no suitable GREG definition" "" }
