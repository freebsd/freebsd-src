#objdump: -drx -Mpower4
#as: -mpower4
#name: Power4 instructions

.*: +file format elf64-powerpc
.*
architecture: powerpc:common64, flags 0x0+11:
HAS_RELOC, HAS_SYMS
start address 0x0+

Sections:
Idx Name +Size +VMA +LMA +File off +Algn
 +0 \.text +0+c4 +0+ +0+ +.*
 +CONTENTS, ALLOC, LOAD, RELOC, READONLY, CODE
 +1 \.data +0+10 +0+ +0+ +.*
 +CONTENTS, ALLOC, LOAD, DATA
 +2 \.bss +0+ +0+ +0+ +.*
 +ALLOC
 +3 \.toc +0+30 +0+ +0+ +.*
 +CONTENTS, ALLOC, LOAD, RELOC, DATA
SYMBOL TABLE:
0+ l +d +\.text	0+ (|\.text)
0+ l +d +\.data	0+ (|\.data)
0+ l +d +\.bss	0+ (|\.bss)
0+ l +\.data	0+ dsym0
0+8 l +\.data	0+ dsym1
0+ l +d +\.toc	0+ (|\.toc)
0+8 l +\.data	0+ usym0
0+10 l +\.data	0+ usym1
0+ +\*UND\*	0+ esym0
0+ +\*UND\*	0+ esym1


Disassembly of section \.text:

0+ <\.text>:
 +0:	e0 83 00 00 	lq      r4,0\(r3\)
			2: R_PPC64_ADDR16_LO_DS	\.data
 +4:	e0 83 00 00 	lq      r4,0\(r3\)
			6: R_PPC64_ADDR16_LO_DS	\.data\+0x8
 +8:	e0 83 00 00 	lq      r4,0\(r3\)
			a: R_PPC64_ADDR16_LO_DS	\.data\+0x8
 +c:	e0 83 00 10 	lq      r4,16\(r3\)
			e: R_PPC64_ADDR16_LO_DS	\.data\+0x10
 +10:	e0 83 00 00 	lq      r4,0\(r3\)
			12: R_PPC64_ADDR16_LO_DS	esym0
 +14:	e0 83 00 00 	lq      r4,0\(r3\)
			16: R_PPC64_ADDR16_LO_DS	esym1
 +18:	e0 82 00 00 	lq      r4,0\(r2\)
			1a: R_PPC64_TOC16_DS	\.toc
 +1c:	e0 82 00 00 	lq      r4,0\(r2\)
			1e: R_PPC64_TOC16_DS	\.toc\+0x8
 +20:	e0 82 00 10 	lq      r4,16\(r2\)
			22: R_PPC64_TOC16_DS	\.toc\+0x10
 +24:	e0 82 00 10 	lq      r4,16\(r2\)
			26: R_PPC64_TOC16_DS	\.toc\+0x18
 +28:	e0 82 00 20 	lq      r4,32\(r2\)
			2a: R_PPC64_TOC16_DS	\.toc\+0x20
 +2c:	e0 82 00 20 	lq      r4,32\(r2\)
			2e: R_PPC64_TOC16_DS	\.toc\+0x28
 +30:	e0 c2 00 20 	lq      r6,32\(r2\)
			32: R_PPC64_TOC16_LO_DS	\.toc\+0x28
 +34:	e0 80 00 00 	lq      r4,0\(0\)
			36: R_PPC64_ADDR16_LO_DS	\.text
 +38:	e0 c3 00 00 	lq      r6,0\(r3\)
			3a: R_PPC64_GOT16_DS	\.data
 +3c:	e0 c3 00 00 	lq      r6,0\(r3\)
			3e: R_PPC64_GOT16_LO_DS	\.data
 +40:	e0 c3 00 00 	lq      r6,0\(r3\)
			42: R_PPC64_PLT16_LO_DS	\.data
 +44:	e0 c3 00 00 	lq      r6,0\(r3\)
			46: R_PPC64_SECTOFF_DS	\.data\+0x8
 +48:	e0 c3 00 00 	lq      r6,0\(r3\)
			4a: R_PPC64_SECTOFF_LO_DS	\.data\+0x8
 +4c:	e0 c4 00 10 	lq      r6,16\(r4\)
 +50:	f8 c7 00 02 	stq     r6,0\(r7\)
 +54:	f8 c7 00 12 	stq     r6,16\(r7\)
 +58:	f8 c7 ff f2 	stq     r6,-16\(r7\)
 +5c:	f8 c7 80 02 	stq     r6,-32768\(r7\)
 +60:	f8 c7 7f f2 	stq     r6,32752\(r7\)
 +64:	00 00 02 00 	attn
 +68:	7c 6f f1 20 	mtcr    r3
 +6c:	7c 6f f1 20 	mtcr    r3
 +70:	7c 68 11 20 	mtcrf   129,r3
 +74:	7c 70 11 20 	mtocrf  1,r3
 +78:	7c 70 21 20 	mtocrf  2,r3
 +7c:	7c 70 41 20 	mtocrf  4,r3
 +80:	7c 70 81 20 	mtocrf  8,r3
 +84:	7c 71 01 20 	mtocrf  16,r3
 +88:	7c 72 01 20 	mtocrf  32,r3
 +8c:	7c 74 01 20 	mtocrf  64,r3
 +90:	7c 78 01 20 	mtocrf  128,r3
 +94:	7c 60 00 26 	mfcr    r3
 +98:	7c 70 10 26 	mfocrf  r3,1
 +9c:	7c 70 20 26 	mfocrf  r3,2
 +a0:	7c 70 40 26 	mfocrf  r3,4
 +a4:	7c 70 80 26 	mfocrf  r3,8
 +a8:	7c 71 00 26 	mfocrf  r3,16
 +ac:	7c 72 00 26 	mfocrf  r3,32
 +b0:	7c 74 00 26 	mfocrf  r3,64
 +b4:	7c 78 00 26 	mfocrf  r3,128
 +b8:	7c 01 17 ec 	dcbz    r1,r2
 +bc:	7c 23 27 ec 	dcbzl   r3,r4
 +c0:	7c 05 37 ec 	dcbz    r5,r6
