#as: --abi=32
#objdump: -dr
#name: Predefined control register names specified in crN syntax.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+240ffd50[ 	]+getcon	sr,r21
[ 	]+4:[ 	]+24dffd50[ 	]+getcon	tea,r21
[ 	]+8:[ 	]+27effd60[ 	]+getcon	ctc,r22
[ 	]+c:[ 	]+248ffd50[ 	]+getcon	spc,r21
[ 	]+10:[ 	]+244ffd50[ 	]+getcon	intevt,r21
[ 	]+14:[ 	]+6d3ffcb0[ 	]+putcon	r19,vbr
[ 	]+18:[ 	]+6e6ffc50[ 	]+putcon	r38,expevt
[ 	]+1c:[ 	]+6d5ffc10[ 	]+putcon	r21,ssr
