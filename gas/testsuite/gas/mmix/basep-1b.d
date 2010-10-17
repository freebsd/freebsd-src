#source: basep-1.s
#as: -linker-allocated-gregs
#objdump: -dr

# Check that this test isn't mistreated with -linker-allocated-gregs.

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <a>:
   0:	0000002a 	trap 0,0,42
   4:	8d2b0034 	ldo \$43,\$0,52
			6: R_MMIX_REG	\.MMIX\.reg_contents
