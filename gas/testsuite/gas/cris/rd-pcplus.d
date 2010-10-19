#as: --em=criself --march=v10 --underscore
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
[ 	]+0:[ 	]+4715 3fbe[ 	]+move \[pc=r7\+r1\.b\],srp
[ 	]+4:[ 	]+6ffd 0000 0100 3f0e[ 	]+move \[pc=pc\+10000 <a\+0x10000>\],p0
[ 	]+c:[ 	]+4385 6f5e[ 	]+move\.d \[pc=r3\+r8\.b\],r5
[ 	]+10:[ 	]+6ffd 0000 0100 6fbe[ 	]+move\.d \[pc=pc\+10000 <a\+0x10000>\],r11
[ 	]+18:[ 	]+6f5d 0000 0a00 3f1e[ 	]+move \[pc=r5\+a0000 <a\+0xa0000>\],vr
[ 	]+20:[ 	]+5f7d 8f02 6fde[ 	]+move\.d \[pc=r7\+655\],r13
[ 	]+26:[ 	]+4161 6fae[ 	]+move\.d \[pc=r6\+65\],r10
[ 	]+2a:[ 	]+0f05[ 	]+nop 
