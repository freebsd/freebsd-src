#objdump: -dr --prefix-addresses --show-raw-insn
#name: ADRL

# Test the `ADRL' pseudo-op

.*: +file format .*arm.*

Disassembly of section .text:
	...
0+2000 <.*> e24f0008 	sub	r0, pc, #8	; 0x8
0+2004 <.*> e2400c20 	sub	r0, r0, #8192	; 0x2000
0+2008 <.*> e28f0020 	add	r0, pc, #32	; 0x20
0+200c <.*> e2800c20 	add	r0, r0, #8192	; 0x2000
0+2010 <.*> e24f0018 	sub	r0, pc, #24	; 0x18
0+2014 <.*> e1a00000 	nop			\(mov r0,r0\)
0+2018 <.*> e28f0008 	add	r0, pc, #8	; 0x8
0+201c <.*> e1a00000 	nop			\(mov r0,r0\)
0+2020 <.*> 028f0000 	addeq	r0, pc, #0	; 0x0
0+2024 <.*> e1a00000 	nop			\(mov r0,r0\)
0+2028 <.*> e24f0030 	sub	r0, pc, #48	; 0x30
0+202c <.*> e2400c20 	sub	r0, r0, #8192	; 0x2000
0+2030 <.*> e28f0c21 	add	r0, pc, #8448	; 0x2100
0+2034 <.*> e1a00000 	nop			\(mov r0,r0\)
	...
0+4030 <.*> e28fec01 	add	lr, pc, #256	; 0x100
	...
	...
