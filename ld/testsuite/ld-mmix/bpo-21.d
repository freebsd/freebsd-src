#source: start.s
#source: bpo-11.s
#source: bpo-7.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#error: ^[^c][^h][^i][^l][^d].* undefined reference to `areg'$

# A BPO reloc against an undefined symbol, with a full set of normal
# BPO:s.

