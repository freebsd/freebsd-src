#objdump: -dr
#name: brokw-1

.*:     file format .*-cris

Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,\$?r0
[	 ]+2:[	 ]+0c00[ 	]+bcc[ ]+(0x10|10 <sym2\+0x10>)
[	 ]+4:[	 ]+4102[ 	]+moveq[ ]+1,\$?r0
[	 ]+6:[	 ]+0ae0[ 	]+ba[ ]+(0x12|12 <next_label>)
[	 ]+8:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+a:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+c:[	 ]+3f0d 1280 0000[ 	]+jump[ ]+(0x8012|8012 <sym1>)
[ 	]+e:[ 	]+(R_CRIS_)?32[ 	]+\.text\+0x[0]*8012
0+12 <next_label>:
[	 ]+12:[	 ]+4202[ 	]+moveq[ ]+2,\$?r0
^[ 	]+\.\.\.
0+8012 <sym1>:
[	 ]+8012:[	 ]+4302[ 	]+moveq[ ]+3,\$?r0
