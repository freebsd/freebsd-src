#objdump: -dr
#name: labfloat

.*:     file format .*-cris

Disassembly of section \.text:

0+ <start>:
[	 ]+0:[	 ]+6f4e 0600 0000[ 	]+move.d 6 <start\+0x6>,\$?r4
[	 ]+2:[ 	]+(R_CRIS_)?32[ 	]+\.text\+0x6
[	 ]+6:[	 ]+ef4e 0600 0000[	 ]+cmp\.d 6 <start\+0x6>,\$?r4
[	 ]+8:[	 ]+(R_CRIS_)?32[	 ]+\.text\+0x6
