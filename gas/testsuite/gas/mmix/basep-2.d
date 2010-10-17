#as: --no-predefined-syms
#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <a>:
   0:	0000002a 	trap 0,0,42
   4:	8d2b0034 	ldo \$43,\$0,52
			6: R_MMIX_REG	\.MMIX\.reg_contents\+0x8
	\.\.\.

0000000000000108 <d>:
 108:	0000001c 	trap 0,0,28
 10c:	8d8f000c 	ldo \$143,\$0,12
			10e: R_MMIX_REG	\.MMIX\.reg_contents
 110:	8df3000c 	ldo \$243,\$0,12
			112: R_MMIX_REG	\.MMIX\.reg_contents\+0x8
 114:	23670028 	addu \$103,\$0,40
			116: R_MMIX_REG	\.MMIX\.reg_contents
 118:	230d0018 	addu \$13,\$0,24
			11a: R_MMIX_REG	\.MMIX\.reg_contents\+0x8
