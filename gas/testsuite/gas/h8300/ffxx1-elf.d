#objdump: --prefix-addresses -dr
#name: FFxx1

# Test for FFxx:8 addressing.

.*:     file format .*h8300.*

Disassembly of section .text:
	...
			0: R_H8_DIR16	main
0+0400 <main> f8 7f             mov.b	#0x7f,r0l
0+0402 <main[+](0x|)2> 28 bb             mov.b	@0xbb:8,r0l
0+0404 <main[+](0x|)4> 6a 88 ff b9       mov.b	r0l,@0xffb9:16
0+0408 <main[+](0x|)8> f8 01             mov.b	#0x1,r0l
0+040a <loop> 6a 88 ff bb       mov.b	r0l,@0xffbb:16
0+040e <delay> 79 01 00 00       mov.w	#0x0,r1
0+0412 <deloop> 0b 01             adds	#0x1,er1
0+0414 <deloop[+](0x|)2> 46 00             bne	.0 \(416\)
			415: R_H8_PCREL8	deloop
0+0416 <deloop[+](0x|)4> 12 88             rotl	r0l
0+0418 <deloop[+](0x|)6> 40 00             bra	.0 \(41a\)
			419: R_H8_PCREL8	loop
	...
