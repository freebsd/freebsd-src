#source: err-bpo2.s
#as: -linker-allocated-gregs
#objdump: -dr

# The -linker-allocated-gregs option validates omissions of GREG.

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <a>:
   0:	0000002a 	trap 0,0,42
   4:	8d2b0000 	ldo \$43,\$0,0
			6: R_MMIX_BASE_PLUS_OFFSET	\.text\+0x34
