	.section ".text"			! [internal]
	.proc	4
	.global	main
	.align	4
	.global	main
main:
!#PROLOGUE# 0
!#PROLOGUE# 1
	save	%sp,-96,%sp
	sethi	%hi(.L18),%o0
	sethi	%hi(msg),%o1
	or	%o1,%lo(msg),%o1	! [internal]
	call	printf,2
	or	%o0,%lo(.L18),%o0	! [internal]
	ret
	restore	%g0,0,%o0
	.type	main,#function
	.size	main,(.-main)
	.section ".data"			! [internal]
	.align	4
Ddata.data:
	.section ".bss"			! [internal]
Bbss.bss:
	.section ".rodata"		! [internal]
Drodata.rodata:
	.file	"hi-sol.c"
	.global	msg
	.global	msg
msg:
	.ascii	"hello, world!\0"
	.type	msg,#object
	.size	msg,14
	.section ".data1", #write, #alloc ! [internal]
	.align	4
.L18:
	.ascii	"%s\n\0"
	.ident	"acomp: (CDS) SPARCompilers 2.0.1 03 Sep 1992"
	.section "text"			! [internal]
	.stabs	"/cygint/s1/users/raeburn/",100,0,0,0
	.stabs	"hi-sol.c",100,0,3,0
	.stabs	"",56,0,0,0
	.stabs	"",56,0,0,0
	.stabs	"Xt ; g ; O ; V=2.0",60,0,0,0x2bb773ba
	.stabs	"char:t(0,1)=bsc1;0;8;",128,0,0,0
	.stabs	"short:t(0,2)=bs2;0;16;",128,0,0,0
	.stabs	"int:t(0,3)=bs4;0;32;",128,0,0,0
	.stabs	"long:t(0,4)=bs4;0;32;",128,0,0,0
	.stabs	"long long:t(0,5)=bs8;0;64;",128,0,0,0
	.stabs	"signed char:t(0,6)=bsc1;0;8;",128,0,0,0
	.stabs	"signed short:t(0,7)=bs2;0;16;",128,0,0,0
	.stabs	"signed int:t(0,8)=bs4;0;32;",128,0,0,0
	.stabs	"signed long:t(0,9)=bs4;0;32;",128,0,0,0
	.stabs	"signed long long:t(0,10)=bs8;0;64;",128,0,0,0
	.stabs	"unsigned char:t(0,11)=buc1;0;8;",128,0,0,0
	.stabs	"unsigned short:t(0,12)=bu2;0;16;",128,0,0,0
	.stabs	"unsigned int:t(0,13)=bu4;0;32;",128,0,0,0
	.stabs	"unsigned long:t(0,14)=bu4;0;32;",128,0,0,0
	.stabs	"unsigned long long:t(0,15)=bu8;0;64;",128,0,0,0
	.stabs	"float:t(0,16)=R1;4;",128,0,0,0
	.stabs	"double:t(0,17)=R2;8;",128,0,0,0
	.stabs	"long double:t(0,18)=R6;16;",128,0,0,0
	.stabs	"void:t(0,19)=bs0;0;0",128,0,0,0
	.stabs	"msg:G(0,20)=ar(0,3);0;13;(0,1)",32,0,14,0
	.stabs	"main:F(0,3);(0,3);(0,21)=*(0,22)=*(0,1)",36,0,0,main
	.stabs	"main",42,0,0,0
	.stabn	192,0,1,0
	.stabn	68,0,4,0
	.stabs	"argc:p(0,3)",160,0,4,68
	.stabs	"argv:p(0,21)",160,0,4,72
	.stabs	"printf:P(0,3)",36,0,0,0
	.stabn	224,0,1,0
	.stabs	"",98,0,0,0
	.section "text"			! [internal]
	.xstabs	".stab.index","/cygint/s1/users/raeburn/",100,0,0,0
	.xstabs	".stab.index","hi-sol.c",100,0,3,0
	.xstabs	".stab.index","",56,0,0,0
	.xstabs	".stab.index","",56,0,0,0
	.xstabs	".stab.index","Xt ; g ; O ; V=2.0",60,0,0,0x2bb773ba
	.xstabs	".stab.index","msg",32,0,0,0
	.xstabs	".stab.index","main",42,0,0,0
	.xstabs	".stab.index","main",36,0,0,0
