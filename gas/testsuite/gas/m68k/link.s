# Test handling of link instruction.
	.text
	.globl	foo
foo:
	link	%a6,&0
	link	%a6,&-4
	link	%a6,&-0x7fff
	link	%a6,&-0x8000
	link	%a6,&-0x8001
	link	%a6,&0x7fff
	link	%a6,&0x8000
	link	%a6,&0x8001
	nop
