# PUSHJ stub border-cases: two with either or both stubs unreachable,
# local symbols, ditto non-local labels, similar with three PUSHJs.

Main	SWYM

	.irp x,0,1,2,3,4,5,6,7,8,9,10,11,12

	.section .text.a\x,"ax"
aa\x:	.space 4,0
a\x:	.space 65536*4,0
	PUSHJ $33,aa\x
	PUSHJ $22,a\x
	.space 65535*4-4*\x

	.section .text.b\x,"ax"
bbb\x:	.space 4,0
bb\x:	.space 4,0
b\x:	.space 65535*4
	PUSHJ $12,bbb\x
	PUSHJ $13,bb\x
	PUSHJ $14,b\x
	.space 65535*4-4*\x

	.section .text.c\x,"ax"
c\x:	PUSHJ $100,ca\x
	PUSHJ $101,cb\x
	.space 65535*4-4*\x

	.section .text.d\x,"ax"
d\x:	PUSHJ $99,da\x
	PUSHJ $98,db\x
	PUSHJ $97,dc\x
	.space 65535*4-4*\x

	.endr
