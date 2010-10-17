# name: ELF MIPS2 markings
# source: empty.s
# objdump: -p
# as: -32 -march=mips2

.*:.*file format.*elf.*mips.*
private flags = 1.......: .*\[mips2\].*

