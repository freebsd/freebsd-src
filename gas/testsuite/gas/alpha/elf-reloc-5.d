#objdump: -dr
#name: alpha elf-reloc-5

.*:     file format elf64-alpha.*

Disassembly of section \.text:

0*0000000 <_start>:
   0:	05 00 e0 c3 	br	18 <nopv>
   4:	04 00 e0 c3 	br	18 <nopv>
   8:	04 00 e0 c3 	br	1c <stdgp>
   c:	05 00 e0 c3 	br	24 <stdgp\+0x8>
  10:	00 00 e0 c3 	br	14 <_start\+0x14>
			10: BRSGP	undef
  14:	00 00 e0 c3 	br	18 <nopv>
			14: BRSGP	extern

0*0000018 <nopv>:
  18:	1f 04 ff 47 	nop	

0*000001c <stdgp>:
  1c:	00 00 bb 27 	ldah	gp,0\(t12\)
			1c: GPDISP	\.text\+0x4
  20:	00 00 bd 23 	lda	gp,0\(gp\)
  24:	1f 04 ff 47 	nop	

0*0000028 <extern>:
  28:	1f 04 ff 47 	nop	
