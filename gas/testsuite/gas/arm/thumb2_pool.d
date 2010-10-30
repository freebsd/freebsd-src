# as: -march=armv6t2
# objdump: -dr --prefix-addresses --show-raw-insn
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]+> 4e04      	ldr	r6, \[pc, #16\]	\(00+14 <[^>]+>\)
0+002 <[^>]+> 4904      	ldr	r1, \[pc, #16\]	\(00+14 <[^>]+>\)
0+004 <[^>]+> f8df 600c 	ldr\.w	r6, \[pc, #12\]	; 00+14 <[^>]+>
0+008 <[^>]+> f8df 9008 	ldr\.w	r9, \[pc, #8\]	; 00+14 <[^>]+>
0+00c <[^>]+> bf00      	nop
0+00e <[^>]+> f8df 5004 	ldr\.w	r5, \[pc, #4\]	; 00+14 <[^>]+>
0+012 <[^>]+> 4900      	ldr	r1, \[pc, #0\]	\(00+14 <[^>]+>\)
0+014 <[^>]+> 12345678 ?	.word	0x12345678
