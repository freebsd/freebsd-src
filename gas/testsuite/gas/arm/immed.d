# name: immediate expressions
# as:
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+0000 <[^>]+> e3a00000 ?	mov	r0, #0	; 0x0
0+0004 <[^>]+> e3e00003 ?	mvn	r0, #3	; 0x3
0+0008 <[^>]+> e51f0010 ?	ldr	r0, \[pc, #-16\]	; 0+0 <[^>]+>
0+000c <[^>]+> e51f0014 ?	ldr	r0, \[pc, #-20\]	; 0+0 <[^>]+>
	\.\.\.
0+1010 <[^>]+> e3a00008 ?	mov	r0, #8	; 0x8
0+1014 <[^>]+> e59f00e4 ?	ldr	r0, \[pc, #228\]	; 0+1100 <[^>]+>
0+1018 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+101c <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
