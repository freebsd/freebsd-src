# name: ELF MIPS16 ASE markings
# source: empty.s
# objdump: -p
# as: -32 -mips16

.*:.*file format.*mips.*
private flags = [0-9a-f]*[4-7c-f]......: .*[[,]mips16[],].*

