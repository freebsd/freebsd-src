#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <here>:
[ 	]+0:[ 	]+3f1e fafc fdfe[ 	]+move fefdfcfa <here\+0xfefdfcfa>,vr
[ 	]+6:[ 	]+3f2e 11ba 0ff0[ 	]+move f00fba11 <here\+0xf00fba11>,pid
[ 	]+c:[ 	]+3f3e 0000 0000[ 	]+move 0 <here>,srs
[ 	]+e:[ 	]+R_CRIS_32	extsym
[ 	]+12:[ 	]+3f4e 0000 0000[ 	]+move 0 <here>,wz
[ 	]+14:[ 	]+R_CRIS_32	extsym2
[ 	]+18:[ 	]+3f5e e903 0000[ 	]+move 3e9 <here\+0x3e9>,exs
[ 	]+1e:[ 	]+3f6e 6500 0000[ 	]+move 65 <here\+0x65>,eda
