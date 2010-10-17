#as:
#objdump: -dr
#name: gcc

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	30 31 ff ff 	mov r0,#0xffff
   4:	30 31 ff ff 	mov r0,#0xffff
   8:	40 31 00 00 	add r0,#0x0
			a: R_XSTORMY16_16	some_external_symbol
   c:	30 31 00 00 	mov r0,#0x0
			e: R_XSTORMY16_16	some_external_symbol
