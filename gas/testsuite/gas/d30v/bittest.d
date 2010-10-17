#objdump: -dr
#name: D30V bittest opt
#as: -WO

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <.text>:
   0:	00f00000 84401083 	nop		->	ldw.s	r1, @\(r2, r3\)
   8:	04406144 00f00000 	ldw.s	r6, @\(r5, r4\)	||	nop	
  10:	00f00000 82201083 	nop		->	bset	r1, r2, r3
  18:	80f00000 02001083 	nop		<-	btst	f1, r2, r3
  20:	00f00000 02301083 	nop		||	bclr	r1, r2, r3
  28:	00f00000 82101083 	nop		->	bnot	r1, r2, r3
  30:	02101083 80f00000 	bnot	r1, r2, r3	->	nop	
  38:	047c0105 02201083 	moddec	r4, 0x5	||	bset	r1, r2, r3
  40:	02201083 847c0105 	bset	r1, r2, r3	->	moddec	r4, 0x5
  48:	02201083 08c04146 	bset	r1, r2, r3	||	joinll.s	r4, r5, r6
  50:	02201083 08c04146 	bset	r1, r2, r3	||	joinll.s	r4, r5, r6