#objdump: -dr

# Catches an error in the relaxation machinery and checks that there's no
# confusion between section offset and absolute address.

.*:     file format .*-cris

Disassembly of section \.text:

0+ <\.text>:
[ 	]+0:[ 	]+08e0[ 	]+ba 0xa
[ 	]+2:[ 	]+0f05[ 	]+nop 
[ 	]+4:[ 	]+3f0d 00db ba00[ 	]+jump 0xbadb00
[ 	]+a:[ 	]+f970[ 	]+bmi 0x4
[ 	]+c:[ 	]+08e0[ 	]+ba 0x16
[ 	]+e:[ 	]+0f05[ 	]+nop 
[ 	]+10:[ 	]+3f0d 000b 0000[ 	]+jump 0xb00
[ 	]+16:[ 	]+f970[ 	]+bmi 0x10
[ 	]+18:[ 	]+08e0[ 	]+ba 0x22
[ 	]+1a:[ 	]+0f05[ 	]+nop 
[ 	]+1c:[ 	]+3f0d 4200 0000[ 	]+jump 0x42
[ 	]+22:[ 	]+f970[ 	]+bmi 0x1c
