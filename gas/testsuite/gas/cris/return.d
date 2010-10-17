#objdump: -dr
#name: return

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+7fb6[ 	]+ret[ ]*
[	 ]+2:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+4:[	 ]+7fa6[ 	]+reti[ ]*
[	 ]+6:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+8:[	 ]+7fe6[ 	]+retb[ ]*
[	 ]+a:[	 ]+0f05[ 	]+nop[ ]*
