# name: ELF MIPS32r2 markings
# source: empty.s
# objdump: -p
# as: -32 -march=mips32r2

.*:.*file format.*elf.*mips.*
private flags = 7.......: .*\[mips32r2\].*

