#objdump: -dr -M gpr-names=numeric -m mips:7000
#name: MIPS RM7000
#as: -march=rm7000

.*: +file format .*mips.*

Disassembly of section \.text:

0+000 <\.text>:
 * 0:	70a62002 *	mul	\$4,\$5,\$6
 * 4:	70850000 *	mad	\$4,\$5
 * 8:	70a60001 *	madu	\$5,\$6
 * c:	00003812 *	mflo	\$7
 *10:	01000011 *	mthi	\$8
	...
