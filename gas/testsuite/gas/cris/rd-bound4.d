#as: --underscore --em=criself --march=common_v10_v32
#source: rd-bound1.s
#objdump: -dr

# Bound with register and immediate are part of the common
# v10+v32 subset.

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <x>:
[ 	]+0:[ 	]+c375[ 	]+bound\.b r3,r7
[ 	]+2:[ 	]+d815[ 	]+bound\.w r8,r1
[ 	]+4:[ 	]+eb35[ 	]+bound\.d r11,r3
[ 	]+6:[ 	]+cf2d 4200[ 	]+bound\.b 0x42,r2
[ 	]+a:[ 	]+df0d 6810[ 	]+bound\.w 0x1068,r0
[ 	]+e:[ 	]+ef5d 6a16 4000[ 	]+bound.d 40166a <x\+0x40166a>,r5
