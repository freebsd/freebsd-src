#source: pushpopv32.s
#as: --underscore --march=common_v10_v32 --em=criself
#objdump: -dr

.*:[ 	]+file format .*-cris

Disassembly of section \.text:
0+ <start>:
[ 	]+0:[ 	]+84e2[ 	]+subq 4,sp
[ 	]+2:[ 	]+eeab[ 	]+move\.d r10,\[sp\]
[ 	]+4:[ 	]+84e2[ 	]+subq 4,sp
[ 	]+6:[ 	]+7eba[ 	]+move srp,\[sp\]
[ 	]+8:[ 	]+6eae[ 	]+move\.d \[sp\+\],r10
[ 	]+a:[ 	]+3ebe[ 	]+move \[sp\+\],srp
