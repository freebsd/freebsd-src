#objdump: -dr -mmips:isa32 -mmips:16
#as: -march=mips32 -mips16 -32
#name: mips16e jalrc/jrc

.*: +file format .*mips.*

Disassembly of section .text:
00000000 <.text>:
   0:[ 	]+eac0[ 	]+jalrc[ 	]+v0
   2:[ 	]+e8a0[ 	]+jrc[ 	]+ra
   4:[ 	]+6a01[ 	]+li[ 	]+v0,1
   6:[ 	]+6500[ 	]+nop
   8:[ 	]+6500[ 	]+nop
   a:[ 	]+6500[ 	]+nop
   c:[ 	]+6500[ 	]+nop
   e:[ 	]+6500[ 	]+nop
