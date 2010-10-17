#source: start.s
#source: bpo-7.s
#source: greg-1.s
#as: -linker-allocated-gregs
#ld: -m mmo
#error: base-plus-offset relocation against register symbol

# Check that we get an error message if we see a BPO against a register
# symbol.  Variant 1: a GREG allocated register.
