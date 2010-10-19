#objdump: -dr
#as: --pic
#source: brokw-2.s

.*:     file format .*-cris
Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,\$?r0
[	 ]+2:[	 ]+1600[ 	]+bcc[ ]+(0x1a|1a <sym2\+0x1a>)
[	 ]+4:[	 ]+0e00[ 	]+bcc[ ]+(0x14|14 <sym2\+0x14>)
[	 ]+6:[	 ]+4102[ 	]+moveq[ ]+1,\$?r0
[	 ]+8:[	 ]+14e0[ 	]+ba[ ]+(0x1e|1e <next_label>)
[	 ]+a:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+c:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+e:[	 ]+6ffd 0c80 0000 3f0e[ 	]+move \[\$?pc=\$?pc\+800c <next_label\+0x7fee>\],\$?p0
[	 ]+16:[	 ]+6ffd 0280 0000 3f0e[ 	]+move \[\$?pc=\$?pc\+8002 <next_label\+0x7fe4>\],\$?p0
0+1e <next_label>:
[	 ]+1e:[	 ]+4202[ 	]+moveq[ ]+2,\$?r0
^[ 	]+\.\.\.
0+801e <sym1>:
[	 ]+801e:[	 ]+4302[ 	]+moveq[ ]+3,\$?r0
0+8020 <sym3>:
[	 ]+8020:[	 ]+4402[ 	]+moveq[ ]+4,\$?r0
^[ 	]+\.\.\.
