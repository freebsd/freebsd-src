#objdump: -D
#name: D10V supported indirect addressing
#as:

.*: +file format elf32-d10v

Disassembly of section .text:

00000000 <main>:
   0:	78 02 72 04 	ldb	r0, @r2	->	ldub	r0, @r2
   4:	70 02 62 04 	ld	r0, @r2	->	ld2w	r0, @r2
   8:	7c 02 68 04 	stb	r0, @r2	->	st	r0, @r2
   c:	75 02 60 05 	st2w	r0, @r2	->	ld	r0, @r2\+
  10:	71 02 e8 05 	ld2w	r0, @r2\+	->	st	r0, @r2\+
  14:	75 02 e4 05 	st2w	r0, @r2\+	->	ld	r0, @r2-
  18:	73 02 ec 05 	ld2w	r0, @r2-	->	st	r0, @r2-
  1c:	77 02 f0 1e 	st2w	r0, @r2-	->	ldb	r0, @sp
  20:	79 0f 60 1e 	ldub	r0, @sp	->	ld	r0, @sp
  24:	71 0f 78 1e 	ld2w	r0, @sp	->	stb	r0, @sp
  28:	74 0f 6a 1e 	st	r0, @sp	->	st2w	r0, @sp
  2c:	70 0f e2 1f 	ld	r0, @sp\+	->	ld2w	r0, @sp\+
  30:	74 0f ea 1f 	st	r0, @sp\+	->	st2w	r0, @sp\+
  34:	72 0f e6 1f 	ld	r0, @sp-	->	ld2w	r0, @sp-
  38:	76 0f ee 1f 	st	r0, @-sp	->	st2w	r0, @-sp
  3c:	f8 02 80 00 	ldb	r0, @\(-0x8000, r2\)
  40:	f9 02 80 00 	ldub	r0, @\(-0x8000, r2\)
  44:	f0 02 80 00 	ld	r0, @\(-0x8000, r2\)
  48:	f1 02 80 00 	ld2w	r0, @\(-0x8000, r2\)
  4c:	fc 02 80 00 	stb	r0, @\(-0x8000, r2\)
  50:	f4 02 80 00 	st	r0, @\(-0x8000, r2\)
  54:	f5 02 80 00 	st2w	r0, @\(-0x8000, r2\)
  58:	26 0d 5e 00 	jmp	r13	||	nop	
