#as:
#objdump: -dr
#name: reloc-2

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	10 30       	inc r0,#0x1
   2:	00 e3       	set1 0x0,#0x1
   4:	00 7c fc 1f 	bn 0x0,#0x1,0x4
   8:	40 31 01 00 	add r0,#0x1
   c:	08 71 01 00 	mov\.w r0,\(r0,1\)
  10:	01 79 00 00 	mov\.w 0x1,#0x0
  14:	01 47       	mov Rx,#0x1
  16:	00 79 01 00 	mov\.w 0x0,#0x1
  1a:	01 02 00 00 	jmpf 0x1
  1e:	ff d0       	bge 0x1f
  20:	fd c0 00 00 	bge Rx,#0x0,0x21
  24:	00 0d fd 0f 	bge r0,r0,0x25
  28:	fe 1f       	br 0x28
  2a:	00 79 00 00 	mov\.w 0x0,#0x0
			2a: R_XSTORMY16_8	global
  2e:	00 47       	mov Rx,#0x0
			2e: R_XSTORMY16_8	global
  30:	00 79 00 00 	mov\.w 0x0,#0x0
			32: R_XSTORMY16_16	global
  34:	fe d0       	bge 0x34
			34: R_XSTORMY16_PC8	global\+0xfffffffe
  36:	fc c0 00 00 	bge Rx,#0x0,0x36
			36: R_XSTORMY16_PC8	global\+0xfffffffc
  3a:	00 0d fc 0f 	bge r0,r0,0x3a
			3c: R_XSTORMY16_REL_12	global\+0xfffffffe
  3e:	fe 1f       	br 0x3e
			3e: R_XSTORMY16_REL_12	global\+0xfffffffe
  40:	0a d0       	bge 0x4c
  42:	06 c0 00 00 	bge Rx,#0x0,0x4c
  46:	00 0d 02 00 	bge r0,r0,0x4c
  4a:	00 10       	br 0x4c
  4c:	fe d0       	bge 0x4c
  4e:	fa c0 00 00 	bge Rx,#0x0,0x4c
  52:	00 0d f6 0f 	bge r0,r0,0x4c
  56:	f4 1f       	br 0x4c
  58:	00 79 00 00 	mov\.w 0x0,#0x0
			5a: R_XSTORMY16_16	global\+0x4
  5c:	00 79 00 00 	mov\.w 0x0,#0x0
			5e: R_XSTORMY16_16	\.text\+0x4c
  60:	00 79 00 00 	mov\.w 0x0,#0x0
			62: R_XSTORMY16_16	\.text\+0x50
  64:	00 79 00 00 	mov\.w 0x0,#0x0
			66: R_XSTORMY16_PC16	global\+0xffffff9c
  68:	00 79 00 00 	mov\.w 0x0,#0x0
			6a: R_XSTORMY16_PC16	global\+0xffffffb4
  6c:	00 02 00 00 	jmpf 0x0
			6c: R_XSTORMY16_24	global
