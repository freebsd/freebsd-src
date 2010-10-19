#as: --underscore --em=criself --march=v10
#source: bound-err-1.s
#objdump: -dr

# A bound insn with a memory operand is an error for v32, but is
# valid for v10.  Check.

.*:     file format elf32-us-cris
Disassembly of section \.text:
0+ <x>:
[ 	]+0:[ 	]+c379[ 	]+bound\.b \[r3\],r7
[ 	]+2:[ 	]+d81d[ 	]+bound\.w \[r8\+\],r1
[ 	]+4:[ 	]+eb39[ 	]+bound\.d \[r11\],r3
[ 	]+6:[ 	]+0f05[ 	]+nop 
