#as:
#objdump: -dr
#name: no_op_insn

.*: +file format .*

Disassembly of section .text:

00000000 <nop>:
   0:	02 30       	nop

00000002 <retx>:
   2:	03 30       	retx

00000004 <di>:
   4:	04 30       	di

00000006 <ei>:
   6:	05 30       	ei

00000008 <wait>:
   8:	06 30       	wait

0000000a <eiwait>:
   a:	07 30       	eiwait
