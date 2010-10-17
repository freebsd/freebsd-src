#as: -march=mips1 -mabi=32 -KPIC
#objdump: -M gpr-names=numeric -dr
#name: MIPS ELF reloc 14

.*:     file format .*

Disassembly of section \.text:

0+00 <foo>:
   0:	8f840000 	lw	\$4,0\(\$28\)
			0: R_MIPS_CALL16	bar
   4:	8f850000 	lw	\$5,0\(\$28\)
			4: R_MIPS_GOT16	\.text
   8:	00000000 	nop
   c:	24a50014 	addiu	\$5,\$5,20
			c: R_MIPS_LO16	\.text
  10:	24c60001 	addiu	\$6,\$6,1
	\.\.\.
