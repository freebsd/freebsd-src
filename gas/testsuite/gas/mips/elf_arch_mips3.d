# name: ELF MIPS3 markings
# source: empty.s
# objdump: -p
# as: -32 -march=mips3

.*:.*file format.*elf.*mips.*
private flags = 2.......: .*\[mips3\].*

