#objdump: -drw
#name: i386 equates
#stderr: equ.e

.*: +file format .*

Disassembly of section .text:

0+000 <_start>:
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+\$0xffffffff,%eax
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+0xffffffff,%eax
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+\$0x0,%eax[ 	0-9a-f]+:[ 	a-zA-Z0-9_]+xtrn
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+0x0,%eax[ 	0-9a-f]+:[ 	a-zA-Z0-9_]+xtrn
[ 0-9a-f]+:[ 	0-9a-f]+test[ 	]+%ecx,%ecx
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+%fs:\(%ecx,%ecx,4\),%ecx
[ 0-9a-f]+:[ 	0-9a-f]+fadd[ 	]+%st\(1\),%st
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+\$0xfffffffe,%eax
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+0xfffffffe,%eax
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+\$0x0,%eax[ 	0-9a-f]+:[ 	a-zA-Z0-9_]+xtrn
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+0x0,%eax[ 	0-9a-f]+:[ 	a-zA-Z0-9_]+xtrn
[ 0-9a-f]+:[ 	0-9a-f]+test[ 	]+%edx,%edx
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+%gs:\(%edx,%edx,8\),%edx
[ 0-9a-f]+:[ 	0-9a-f]+mov[ 	]+%gs:\(%edx,%edx,8\),%edx
[ 0-9a-f]+:[ 	0-9a-f]+fadd[ 	]+%st\(1\),%st
[ 0-9a-f]+:[ 	0-9a-f]+fadd[ 	]+%st\(7\),%st
#pass
