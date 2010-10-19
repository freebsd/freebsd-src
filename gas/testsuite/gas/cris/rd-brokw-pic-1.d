#objdump: -dr
#as: --pic
#source: brokw-1.s

.*:     file format .*-cris

Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,\$?r0
[	 ]+2:[	 ]+0c00[ 	]+bcc[ ]+(0x10|10 <sym2\+0x10>)
[	 ]+4:[	 ]+4102[ 	]+moveq[ ]+1,\$?r0
[	 ]+6:[	 ]+0ce0[ 	]+ba[ ]+(0x14|14 <next_label>)
[	 ]+8:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+a:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+c:[	 ]+6ffd 0280 0000 3f0e[ 	]+move \[\$?pc=\$?pc\+8002 <next_label\+0x7fee>\],\$?p0
0+14 <next_label>:
[	 ]+14:[	 ]+4202[ 	]+moveq[ ]+2,\$?r0
^[ 	]+\.\.\.
0+8014 <sym1>:
[	 ]+8014:[	 ]+4302[ 	]+moveq[ ]+3,\$?r0
^[ 	]+\.\.\.
