#objdump: -dr
#name: continue

.*:     file format .*-cris

Disassembly of section \.text:

0+ <start>:
[	 ]+0:[	 ]+e87b[ 	]+move.d \$?r7,\[\$?r8\]
[	 ]+2:[	 ]+e89b[ 	]+move.d \$?r9,\[\$?r8\]
