# name: ELF MIPS5 markings
# source: empty.s
# objdump: -p
# as: -32 -march=mips5

.*:.*file format.*elf.*mips.*
private flags = 4.......: .*\[mips5\].*

