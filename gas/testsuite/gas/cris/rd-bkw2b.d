#as: --underscore --em=criself --march=v32
#source: brokw-2.s
#objdump: -dr

.*:     file format .*-cris

Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,r0
[	 ]+2:[	 ]+1600[ 	]+.*
[	 ]+4:[	 ]+0e00[ 	]+.*
[	 ]+6:[	 ]+4102[ 	]+moveq[ ]+1,r0
[	 ]+8:[	 ]+16e0[ 	]+ba[ ]+1e <next_label>
[	 ]+a:[	 ]+b005[ 	]+nop[ ]*
[	 ]+c:[	 ]+b005[ 	]+nop[ ]*
[	 ]+e:[	 ]+bf0e 1280 0000[ 	]+ba[ ]+8020 <sym3>
[	 ]+14:[	 ]+b005[ 	]+nop[ ]*
[	 ]+16:[	 ]+bf0e 0880 0000[ 	]+ba[ ]+801e <sym1>
[	 ]+1c:[	 ]+b005[ 	]+nop[ ]*
0+1e <next_label>:
[	 ]+1e:[	 ]+4202[ 	]+moveq[ ]+2,r0
[ 	]+\.\.\.
0+801e <sym1>:
[	 ]+801e:[	 ]+4302[ 	]+moveq[ ]+3,r0
0+8020 <sym3>:
[	 ]+8020:[	 ]+4402[ 	]+moveq[ ]+4,r0
[ 	]+\.\.\.
