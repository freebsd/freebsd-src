# name: ELF MIPS64r2 markings
# source: empty.s
# objdump: -p
# as: -march=mips64r2

.*:.*file format.*elf.*mips.*
private flags = 8.......: .*\[mips64r2\].*

