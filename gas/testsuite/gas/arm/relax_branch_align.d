#name: Branch relaxation with alignment.
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]+> bf00      	nop
0+002 <[^>]+> f000 8080 	beq.w	0+106 <[^>]*>
0+006 <[^>]+> bf00      	nop
#...
0+100 <[^>]+> bf00      	nop
0+102 <[^>]+> f47f af80 	bne.w	0+006 <[^>]*>
0+106 <[^>]+> bf00      	nop
