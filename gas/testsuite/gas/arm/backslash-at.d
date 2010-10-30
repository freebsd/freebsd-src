#objdump: -dr --prefix-addresses --show-raw-insn
#name: Backslash-at for ARM

.*:     file format .*arm.*

Disassembly of section .text:
0+000 <.*>.*615c.*
0+002 <foo> e3a00000 	mov	r0, #0	; 0x0
0+006 <foo\+0x4> e3a00000 	mov	r0, #0	; 0x0
0+00a <foo\+0x8> e3a00000 	mov	r0, #0	; 0x0
0+00e <foo\+0xc> e3a00001 	mov	r0, #1	; 0x1
0+012 <foo\+0x10> e3a00001 	mov	r0, #1	; 0x1
0+016 <foo\+0x14> e3a00001 	mov	r0, #1	; 0x1
0+01a <foo\+0x18> e3a00002 	mov	r0, #2	; 0x2
0+01e <foo\+0x1c> e3a00002 	mov	r0, #2	; 0x2
0+022 <foo\+0x20> e3a00002 	mov	r0, #2	; 0x2
#...
