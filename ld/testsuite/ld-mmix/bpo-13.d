#source: start.s
#source: bpo-7.s
#source: areg-256.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#error: base-plus-offset relocation against register symbol

# Check that we get an error message if we see a BPO against a register
# symbol.  Variant 2: a register symbol.
