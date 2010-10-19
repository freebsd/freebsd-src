#objdump: --prefix-addresses -dr
#name: FFxx1

# Test for FFxx:8 addressing.

.*:     file format .*h8300.*

Disassembly of section .text:
	...
			0: R_H8_DIR16	main
0+0400 <main>.*mov.b	#0x7f,r0l
0+0402 <.*>.*mov.b	@0xbb:8,r0l
0+0404 <.*>.*mov.b	r0l,@0xffb9:16
0+0408 <.*>.*mov.b	#0x1,r0l
0+040a <loop>.*mov.b	r0l,@0xffbb:16
0+040e <delay>.*mov.w	#0x0,r1
0+0412 <deloop>.*adds	#1,r1
0+0414 <.*>.*bne	.0 \(0x416\)
			415: R_H8_PCREL8	deloop
0+0416 <.*>.*rotl.b	r0l
0+0418 <.*>.*bra	.0 \(0x41a\)
			419: R_H8_PCREL8	loop
	...
