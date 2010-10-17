#source: mips16-1a.s -no-mips16
#source: mips16-1b.s -mips16
#ld: -r
#objdump: -pd

.*:.*file format.*mips.*
private flags = [0-9a-f]*[4-7c-f]......: .*[[,]mips16[],].*

#pass
