#as: -64 -Av9
#objdump: -dr
#name: sparc64 reloc64

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <foo>:
   0:	03 04 8d 15 	sethi  %hi\(0x12345400\), %g1
   4:	82 10 62 78 	or  %g1, 0x278, %g1.*
   8:	01 00 00 00 	nop 
   c:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			c: R_SPARC_HH22	.text
  10:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			10: R_SPARC_HM10	.text
  14:	01 00 00 00 	nop 
  18:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			18: R_SPARC_HH22	.text\+0x1234567800000000
  1c:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			1c: R_SPARC_HM10	.text\+0x1234567800000000
  20:	01 00 00 00 	nop 
  24:	03 3f b7 2e 	sethi  %hi\(0xfedcb800\), %g1
  28:	82 10 62 98 	or  %g1, 0x298, %g1.*
  2c:	05 1d 95 0c 	sethi  %hi\(0x76543000\), %g2
  30:	84 10 62 10 	or  %g1, 0x210, %g2
  34:	01 00 00 00 	nop 
  38:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			38: R_SPARC_HH22	.text
  3c:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			3c: R_SPARC_HM10	.text
  40:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			40: R_SPARC_LM22	.text
  44:	84 10 60 00 	mov  %g1, %g2
			44: R_SPARC_LO10	.text
  48:	01 00 00 00 	nop 
  4c:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			4c: R_SPARC_HH22	.text\+0xfedcba9876543210
  50:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			50: R_SPARC_HM10	.text\+0xfedcba9876543210
  54:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			54: R_SPARC_LM22	.text\+0xfedcba9876543210
  58:	84 10 60 00 	mov  %g1, %g2
			58: R_SPARC_LO10	.text\+0xfedcba9876543210
  5c:	01 00 00 00 	nop 
  60:	03 2a 61 d9 	sethi  %hi\(0xa9876400\), %g1
  64:	82 10 61 43 	or  %g1, 0x143, %g1.*
  68:	82 10 62 10 	or  %g1, 0x210, %g1
  6c:	01 00 00 00 	nop 
  70:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			70: R_SPARC_H44	.text
  74:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			74: R_SPARC_M44	.text
  78:	82 10 60 00 	mov  %g1, %g1
			78: R_SPARC_L44	.text
  7c:	01 00 00 00 	nop 
  80:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			80: R_SPARC_H44	.text\+0xa9876543210
  84:	82 10 60 00 	mov  %g1, %g1	! 0 <foo>
			84: R_SPARC_M44	.text\+0xa9876543210
  88:	82 10 60 00 	mov  %g1, %g1
			88: R_SPARC_L44	.text\+0xa9876543210
  8c:	01 00 00 00 	nop 
  90:	03 22 6a f3 	sethi  %hi\(0x89abcc00\), %g1
  94:	82 18 7e 10 	xor  %g1, -496, %g1
  98:	01 00 00 00 	nop 
  9c:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			9c: R_SPARC_HIX22	.text
  a0:	82 18 60 00 	xor  %g1, 0, %g1
			a0: R_SPARC_LOX10	.text
  a4:	01 00 00 00 	nop 
  a8:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			a8: R_SPARC_HIX22	.text\+0xffffffff76543210
  ac:	82 18 60 00 	xor  %g1, 0, %g1
			ac: R_SPARC_LOX10	.text\+0xffffffff76543210
  b0:	01 00 00 00 	nop 
