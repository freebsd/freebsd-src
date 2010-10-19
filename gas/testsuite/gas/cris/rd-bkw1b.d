#as: --underscore --em=criself --march=v32
#source: brokw-1.s
#objdump: -dr

.*:     file format .*-cris

Disassembly of section \.text:
0+ <sym2>:
[	 ]+0:[	 ]+4002[ 	]+moveq[ ]+0,r0
[	 ]+2:[	 ]+0c00[ 	]+.*
[	 ]+4:[	 ]+4102[ 	]+moveq[ ]+1,r0
[	 ]+6:[	 ]+0ee0[ 	]+ba[ ]+14 <next_label>
[	 ]+8:[	 ]+b005[ 	]+nop[ ]*
[	 ]+a:[	 ]+b005[ 	]+nop[ ]*
[	 ]+c:[	 ]+bf0e 0880 0000[ 	]+ba[ ]+8014 <sym1>
[	 ]+12:[	 ]+b005[ 	]+nop[ ]*
0+14 <next_label>:
[	 ]+14:[	 ]+4202[ 	]+moveq[ ]+2,r0
[ 	]+\.\.\.
0+8014 <sym1>:
[	 ]+8014:[	 ]+4302[ 	]+moveq[ ]+3,r0
[ 	]+\.\.\.

