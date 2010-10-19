#objdump: -dr
#name: pic2

.*: +file format .*

Disassembly of section .text:

0+0000 <pic_gotpc>:
   0:	7e 01 f0 00 	bl 4 <pic_gotpc\+0x4> \|\| nop
   4:	ec 00 00 00 	ld24 r12,0 <pic_gotpc>
			4: R_M32R_GOTPC24	_GLOBAL_OFFSET_TABLE_
   8:	0c ae f0 00 	add r12,lr \|\| nop

0+000c <pic_gotpc_slo>:
   c:	7e 01 f0 00 	bl 10 <pic_gotpc_slo\+0x4> \|\| nop
  10:	dc c0 00 00 	seth r12,[#]0x0
			10: R_M32R_GOTPC_HI_SLO	_GLOBAL_OFFSET_TABLE_
  14:	8c ac 00 00 	add3 r12,r12,[#]0
			14: R_M32R_GOTPC_LO	_GLOBAL_OFFSET_TABLE_\+0x4
  18:	0c ae f0 00 	add r12,lr \|\| nop

0+001c <pic_gotpc_ulo>:
  1c:	7e 01 f0 00 	bl 20 <pic_gotpc_ulo\+0x4> \|\| nop
  20:	dc c0 00 00 	seth r12,[#]0x0
			20: R_M32R_GOTPC_HI_ULO	_GLOBAL_OFFSET_TABLE_
  24:	8c ec 00 00 	or3 r12,r12,[#]0x0
			24: R_M32R_GOTPC_LO	_GLOBAL_OFFSET_TABLE_\+0x4
  28:	0c ae f0 00 	add r12,lr \|\| nop

0+002c <pic_got>:
  2c:	e0 00 00 00 	ld24 r0,0 <pic_gotpc>
			2c: R_M32R_GOTOFF	sym

0+0030 <pic_got16>:
  30:	dc c0 00 00 	seth r12,[#]0x0
			30: R_M32R_GOT16_HI_SLO	sym2
  34:	8c ac 00 00 	add3 r12,r12,[#]0
			34: R_M32R_GOT16_LO	sym2
  38:	dc c0 00 00 	seth r12,[#]0x0
			38: R_M32R_GOTOFF_HI_ULO	sym2
  3c:	8c ec 00 00 	or3 r12,r12,[#]0x0
			3c: R_M32R_GOT16_LO	sym2

0+0040 <pic_plt>:
  40:	fe 00 00 00 	bl 40 <pic_plt>
			40: R_M32R_26_PLTREL	func

0+0044 <gotoff>:
  44:	e0 00 00 00 	ld24 r0,0 <pic_gotpc>
			44: R_M32R_GOTOFF	.text\+0x44
  48:	d0 c0 00 00 	seth r0,[#]0x0
			48: R_M32R_GOTOFF_HI_SLO	.text\+0x44
  4c:	80 a0 00 00 	add3 r0,r0,[#]0
			4c: R_M32R_GOTOFF_LO	.text\+0x44
  50:	d0 c0 00 00 	seth r0,[#]0x0
			50: R_M32R_GOTOFF_HI_ULO	.text\+0x44
  54:	80 e0 00 00 	or3 r0,r0,[#]0x0
			54: R_M32R_GOTOFF_LO	.text\+0x44
