#source: start.s
#source: bpo-10.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#error: Too many global registers

# Check that many too many gregs are recognized (and not signed/unsigned
# bugs with checks for < 32 appear).
