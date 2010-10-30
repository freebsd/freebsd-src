#objdump: -rsj .data
#name: elf equate relocs

.*: +file format .*

RELOCATION RECORDS FOR \[.*\]:
OFFSET *TYPE *VALUE 
0*0 [^ ]+ +(\.bss(\+0x0*4)?|y1)
0*4 [^ ]+ +(\.bss(\+0x0*8)?|y2)
#...
Contents of section .data:
 0000 0[04]00000[04] 0[08]00000[08].*
#pass
