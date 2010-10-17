#as:
#objdump: -dr
#name: fslot

.*: +file format .*

Disassembly of section .text:

0+0 <bl>:
 *0:	7e 00 f0 00 	bl 0 <bl> \|\| nop
 *4:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+8 <bl_s>:
 *8:	7e 00 f0 00 	bl 8 <bl_s> \|\| nop
 *c:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+10 <bra>:
 *10:	7f 00 f0 00 	bra 10 <bra> \|\| nop
 *14:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+18 <bra_s>:
 *18:	7f 00 f0 00 	bra 18 <bra_s> \|\| nop
 *1c:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+20 <jl>:
 *20:	1e c0 f0 00 	jl r0 \|\| nop
 *24:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+28 <trap>:
 *28:	10 f4 f0 00 	trap [#]*0x4 \|\| nop
 *2c:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop
