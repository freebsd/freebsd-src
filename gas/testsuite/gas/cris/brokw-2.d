#objdump: -dr
#name: brokw-2

.*:     file format .*-cris
Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,\$?r0
[	 ]+2:[	 ]+1400[ 	]+bcc[ ]+(0x18|18 <sym2\+0x18>)
[	 ]+4:[	 ]+0e00[ 	]+bcc[ ]+(0x14|14 <sym2\+0x14>)
[	 ]+6:[	 ]+4102[ 	]+moveq[ ]+1,\$?r0
[	 ]+8:[	 ]+10e0[ 	]+ba[ ]+(0x1a|1a <next_label>)
[	 ]+a:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+c:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+e:[	 ]+3f0d 1c80 0000[ 	]+jump[ ]+(0x801c|801c <sym3>)
[ 	]+10:[ 	]+(R_CRIS_)?32[ 	]+\.text\+0x[0]*801c
[	 ]+14:[	 ]+3f0d 1a80 0000[ 	]+jump[ ]+(0x801a|801a <sym1>)
[ 	]+16:[ 	]+(R_CRIS_)?32[ 	]+\.text\+0x[0]*801a
0+1a <next_label>:
[	 ]+1a:[	 ]+4202[ 	]+moveq[ ]+2,\$?r0
^[ 	]+\.\.\.
0+801a <sym1>:
[	 ]+801a:[	 ]+4302[ 	]+moveq[ ]+3,\$?r0
0+801c <sym3>:
[	 ]+801c:[	 ]+4402[ 	]+moveq[ ]+4,\$?r0
^[ 	]+\.\.\.
