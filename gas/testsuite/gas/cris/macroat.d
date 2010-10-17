#objdump: -dr
#name: macroat

.*:     file format .*-cris

Disassembly of section .text:

0+ <start>:
[	 ]+0:[	 ]+ef0e 0500 0000[	 ]+cmp.d 0x5,\$?r0
[	 ]+6:[	 ]+0230[	 ]+beq  0xa
[	 ]+8:[	 ]+0f05[	 ]+nop[	 ]*

0+a <test_gr00000>:
[	 ]+a:[	 ]+0f05[	 ]+nop[	 ]*
