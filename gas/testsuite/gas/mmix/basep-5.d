#source: err-bpo3.s
#as: -linker-allocated-gregs
#objdump: -dr

# The -linker-allocated-gregs option validates omissions of GREG.

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <a>:
   0:	0000002a 	trap 0,0,42
   4:	8d2b0000 	ldo \$43,\$0,0
			6: R_MMIX_BASE_PLUS_OFFSET	\.text\+0x34
	\.\.\.

0+108 <d>:
 108:	0000001c 	trap 0,0,28
 10c:	8d8f0000 	ldo \$143,\$0,0
			10e: R_MMIX_BASE_PLUS_OFFSET	\.text\+0x114
 110:	8df30000 	ldo \$243,\$0,0
			112: R_MMIX_BASE_PLUS_OFFSET	\.text\+0xc
 114:	23670000 	addu \$103,\$0,0
			116: R_MMIX_BASE_PLUS_OFFSET	\.text\+0x130
 118:	230d0000 	addu \$13,\$0,0
			11a: R_MMIX_BASE_PLUS_OFFSET	\.text\+0x18
