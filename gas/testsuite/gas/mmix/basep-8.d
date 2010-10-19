#as: -linker-allocated-gregs
#objdump: -drt

# Since we don't merge BPO-relocs until linking with
# -linker-allocated-gregs, we automatically correctly handle the two
# seemingly neighboring comm-symbols that don't merge well at
# assembly-time.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l     O \.bss	0+4 comm_symbol3
0+4 l     O \.bss	0+4 comm_symbol4
0+4       O \*COM\*	0+4 comm_symbol1
0+4       O \*COM\*	0+4 comm_symbol2

Disassembly of section \.text:

0+ <\.text>:
   0:	232a0000 	addu \$42,\$0,0
			2: R_MMIX_BASE_PLUS_OFFSET	comm_symbol1
   4:	232b0000 	addu \$43,\$0,0
			6: R_MMIX_BASE_PLUS_OFFSET	comm_symbol2
   8:	232c0000 	addu \$44,\$0,0
			a: R_MMIX_BASE_PLUS_OFFSET	\.bss
   c:	232d0000 	addu \$45,\$0,0
			e: R_MMIX_BASE_PLUS_OFFSET	\.bss\+0x4
