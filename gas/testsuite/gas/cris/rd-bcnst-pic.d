#objdump: -dr
#as: --pic --underscore --em=criself
#source: rd-bcnst.s

# Catches an error in the relaxation machinery and checks that there's no
# confusion between section offset and absolute address.

.*:     file format elf32.*-cris

Disassembly of section \.text:

0+ <\.text>:
[ 	]+0:[ 	]+0ae0[ 	]+ba 0xc
[ 	]+2:[ 	]+0f05[ 	]+nop 
[ 	]+4:[ 	]+6ffd 0000 0000 3f0e[ 	]+move \[pc=pc\+0x0\],p0
[ 	]+6:[ 	]+R_CRIS_32_PCREL[ 	]+\*ABS\*\+0xbadb00
[ 	]+c:[ 	]+f770[ 	]+bmi 0x4
[ 	]+e:[ 	]+0ae0[ 	]+ba 0x1a
[ 	]+10:[ 	]+0f05[ 	]+nop 
[ 	]+12:[ 	]+6ffd 0000 0000 3f0e[ 	]+move \[pc=pc\+0x0\],p0
[ 	]+14:[ 	]+R_CRIS_32_PCREL[ 	]+\*ABS\*\+0xb00
[ 	]+1a:[ 	]+f770[ 	]+bmi 0x12
[ 	]+1c:[ 	]+0ae0[ 	]+ba 0x28
[ 	]+1e:[ 	]+0f05[ 	]+nop 
[ 	]+20:[ 	]+6ffd 0000 0000 3f0e[ 	]+move \[pc=pc\+0x0\],p0
[ 	]+22:[ 	]+R_CRIS_32_PCREL[ 	]+\*ABS\*\+0x42
[ 	]+28:[ 	]+f770[ 	]+bmi 0x20
[ 	]+2a:[ 	]+0000[ 	]+bcc \.\+2
