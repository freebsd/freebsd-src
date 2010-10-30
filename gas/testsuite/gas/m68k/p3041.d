#name: PR 3041
#objdump: -dr

.*:     file format .*

Disassembly of section .text:

0+ <.*>:
   0:	4ef9 0000 0002 [ 	]+jmp 2 <mylabel-0x6>
			2: .*	mylabel
   6:	4e71 [ 	]+nop

0+8 <mylabel>:
   8:	4e71 [ 	]+nop
   a:	4e71 [ 	]+nop
