	.text
	.stabs "int:t(0,1)=r(0,1);-2147483648;2147483647;",128,0,0,0
	.stabs "char:t(0,2)=r(0,2);0;127;",128,0,0,0

	.section	.myinit,"ax"
	.stabs "barxfoo:F(0,20)",36,0,2,_bar
	.global _bar
	.global _start
_start:
_bar:
	.long 123

	.section	.mytext.baz,"ax"
	.stabs "baz:F(0,20)",36,0,6,_baz
	.global _baz
_baz:
	.long 456
