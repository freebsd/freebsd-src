#as: -m32rx
#objdump: -dr
#name: fslotx

.*: +file format .*

Disassembly of section .text:

0+0 <bcl>:
 *0:	78 00 f0 00 	bcl 0 <bcl> \|\| nop
 *4:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+8 <bcl_s>:
 *8:	78 00 f0 00 	bcl 8 <bcl_s> \|\| nop
 *c:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+10 <bncl>:
  10:	79 00 f0 00 	bncl 10 <bncl> \|\| nop
  14:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop

0+18 <bncl_s>:
  18:	79 00 f0 00 	bncl 18 <bncl_s> \|\| nop
  1c:	60 08 f0 00 	ldi r0,[#]*8 \|\| nop
