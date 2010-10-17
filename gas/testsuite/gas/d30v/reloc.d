#objdump: -dr
#name: D30V relocation test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <start>:
       0:	88082000 80000028 	add.l	r2, r0, 0x28
			0: R_D30V_32	.text
       8:	88084000 80000000 	add.l	r4, r0, 0x0
			8: R_D30V_32	.data
      10:	88084000 80000006 	add.l	r4, r0, 0x6
			10: R_D30V_32	.data
      18:	88084000 80000000 	add.l	r4, r0, 0x0
			18: R_D30V_32	unk
      20:	80080000 80000018 	bra.l	18	\(38 <cont>\)

00000028 <hello>:
      28:	48656c6c 6f20576f 	.long	0x48656c6c	||	.long	0x6f20576f
      30:	726c640a 00f00000 	.long	0x726c640a	||	nop	

00000038 <cont>:
      38:	80180000 80000048 	jmp.l	48 <cont2>
			38: R_D30V_32	.text
      40:	088020c0 00f00000 	abs	r2, r3	||	nop	

00000048 <cont2>:
      48:	000bfff7 00f00000 	bra.s	-48	\(0 <start>\)	||	nop	
      50:	00080205 00f00000 	bra.s	1028	\(1078 <exit>\)	||	nop	
      58:	00180000 00f00000 	jmp.s	0 <start>	||	nop	
      60:	006c1ffb 00f00000 	bsrtnz.s	r1, -28	\(38 <cont>\)	||	nop	
      68:	006c1ffa 00f00000 	bsrtnz.s	r1, -30	\(38 <cont>\)	||	nop	
      70:	004c1ff9 00f00000 	bratnz.s	r1, -38	\(38 <cont>\)	||	nop	
      78:	004c1ff8 00f00000 	bratnz.s	r1, -40	\(38 <cont>\)	||	nop	
      80:	005c1007 00f00000 	jmptnz.s	r1, 38 <cont>	||	nop	
			80: R_D30V_15	.text
      88:	006c11f1 00f00000 	bsrtnz.s	r1, f88	\(1010 <foo>\)	||	nop	
      90:	005c1000 00f00000 	jmptnz.s	r1, 0 <start>	||	nop	
			90: R_D30V_15	unk
      98:	006c1000 00f00000 	bsrtnz.s	r1, 0	\(98 <cont2\+0x50>\)	||	nop	
			98: R_D30V_15_PCREL	unk
      a0:	805c1000 80000000 	jmptnz.l	r1, 0 <start>
			a0: R_D30V_32	unk
      a8:	806c1000 80000000 	bsrtnz.l	r1, 0	\(a8 <cont2\+0x60>\)
			a8: R_D30V_32_PCREL	unk
      b0:	000801ec 00f00000 	bra.s	f60	\(1010 <foo>\)	||	nop	
      b8:	80080000 80000f58 	bra.l	f58	\(1010 <foo>\)
      c0:	000bffe8 00f00000 	bra.s	-c0	\(0 <start>\)	||	nop	
      c8:	80180000 80000000 	jmp.l	0 <start>
			c8: R_D30V_32	.text
      d0:	80180000 80000000 	jmp.l	0 <start>
			d0: R_D30V_32	.text
      d8:	00180000 00f00000 	jmp.s	0 <start>	||	nop	
			d8: R_D30V_21	.text
      e0:	00180202 00f00000 	jmp.s	1010 <foo>	||	nop	
			e0: R_D30V_21	.text
      e8:	000bffe3 00f00000 	bra.s	-e8	\(0 <start>\)	||	nop	
      f0:	80080000 80000000 	bra.l	0	\(f0 <cont2\+0xa8>\)
			f0: R_D30V_32_PCREL	unknown
      f8:	80180000 80000000 	jmp.l	0 <start>
			f8: R_D30V_32	unknown
     100:	00180000 00f00000 	jmp.s	0 <start>	||	nop	
			100: R_D30V_21	unknown
     108:	00080000 00f00000 	bra.s	0	\(108 <cont2\+0xc0>\)	||	nop	
			108: R_D30V_21_PCREL	unknown
	...

00001010 <foo>:
    1010:	08001000 00f00000 	add.s	r1, r0, r0	||	nop	
    1018:	846bc000 80001070 	ld2w.l	r60, @\(r0, 0x1070\)
			1018: R_D30V_32	.text
    1020:	0803e000 8028000b 	add.s	r62, r0, r0	->	bsr.s	58	\(1078 <exit>\)
    1028:	002bfffd 00f00000 	bsr.s	-18	\(1010 <foo>\)	||	nop	
    1030:	000bfe03 00f00000 	bra.s	-fe8	\(48 <cont2>\)	||	nop	
    1038:	000bfe02 00f00000 	bra.s	-ff0	\(48 <cont2>\)	||	nop	
    1040:	00280007 00f00000 	bsr.s	38	\(1078 <exit>\)	||	nop	
    1048:	0018020f 00f00000 	jmp.s	1078 <exit>	||	nop	
			1048: R_D30V_21	.text
    1050:	0018020f 00f00000 	jmp.s	1078 <exit>	||	nop	
			1050: R_D30V_21	.text
    1058:	0018020f 00f00000 	jmp.s	1078 <exit>	||	nop	
			1058: R_D30V_21	.text
    1060:	80280000 80000018 	bsr.l	18	\(1078 <exit>\)
    1068:	80180000 80001078 	jmp.l	1078 <exit>
			1068: R_D30V_32	.text

00001070 <longzero>:
	...

00001078 <exit>:
    1078:	0010003e 00f00000 	jmp.s	r62	||	nop	
