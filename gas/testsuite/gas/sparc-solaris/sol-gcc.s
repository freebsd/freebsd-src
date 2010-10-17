	.file	"hi-sol.c"
.stabs "/1h/devo/src/gas/testsuite/gas/",100,0,0,.LLtext0
.stabs "hi-sol.c",100,0,0,.LLtext0
.section	".text"
.LLtext0:
	.stabs	"gcc2_compiled.", 0x3c, 0, 0, 0
.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
.stabs "char:t2=r2;0;127;",128,0,0,0
.stabs "long int:t3=r1;-2147483648;2147483647;",128,0,0,0
.stabs "unsigned int:t4=r1;0;-1;",128,0,0,0
.stabs "long unsigned int:t5=r1;0;-1;",128,0,0,0
.stabs "short int:t6=r1;-32768;32767;",128,0,0,0
.stabs "long long int:t7=r1;0;-1;",128,0,0,0
.stabs "short unsigned int:t8=r1;0;65535;",128,0,0,0
.stabs "long long unsigned int:t9=r1;0;-1;",128,0,0,0
.stabs "signed char:t10=r1;-128;127;",128,0,0,0
.stabs "unsigned char:t11=r1;0;255;",128,0,0,0
.stabs "float:t12=r1;4;0;",128,0,0,0
.stabs "double:t13=r1;8;0;",128,0,0,0
.stabs "long double:t14=r1;8;0;",128,0,0,0
.stabs "void:t15=15",128,0,0,0
.stabs "msg:G16=ar1;0;13;2",32,0,0,0
	.global msg
.section	".rodata"
	.align 8
	.type	 msg,#object
	.size	 msg,14
msg:
	.asciz	"hello, world!"
	.align 8
.LLC0:
	.asciz	"%s\n"
.section	".text"
	.align 4
.stabs "main:F1",36,0,0,main
.stabs "argc:P1",64,0,0,24
.stabs "argv:P17=*18=*2",64,0,0,25
	.global main
	.type	 main,#function
	.proc	04
main:
.stabn 68,0,4,.LM1-main
.LM1:
	!#PROLOGUE# 0
	save %sp,-112,%sp
	!#PROLOGUE# 1
.stabn 68,0,5,.LM2-main
.LM2:
.LLBB2:
	sethi %hi(.LLC0),%o0
	or %o0,%lo(.LLC0),%o0
	sethi %hi(msg),%o1
	call printf,0
	or %o1,%lo(msg),%o1
.stabn 68,0,6,.LM3-main
.LM3:
.stabn 68,0,7,.LM4-main
.LM4:
.LLBE2:
	ret
	restore %g0,0,%o0
.LLfe1:
	.size	 main,.LLfe1-main
.stabn 192,0,0,.LLBB2-main
.stabn 224,0,0,.LLBE2-main
	.ident	"GCC: (GNU) cygnus-2.3.3"
