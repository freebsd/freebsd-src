#as: -march=mips2 -mabi=32
#objdump: -M gpr-names=numeric -dr
#name: MIPS expression 1

.*:     file format .*

Disassembly of section \.text:

0+00 <foo>:
   0:	8c840000 	lw	\$4,0\(\$4\)
			0: R_MIPS_LO16	foo
   4:	8c840038 	lw	\$4,56\(\$4\)
   8:	8c840008 	lw	\$4,8\(\$4\)
			8: R_MIPS_LO16	foo
   c:	8c840008 	lw	\$4,8\(\$4\)
			c: R_MIPS_LO16	foo
  10:	8c840008 	lw	\$4,8\(\$4\)
			10: R_MIPS_LO16	foo
	\.\.\.
