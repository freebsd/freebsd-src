	/* Force .got aligned to 4K, so it very likely gets at 0x804a100
	   (0x60 bytes .tdata and 0xa0 bytes .dynamic)  */
	.data
	.balign	4096
	.section ".tdata", "awT", @progbits
	.globl foo
foo:	.long 27

	/* Force .text aligned to 4K, so it very likely gets at 0x8049000.  */
	.text
	.balign	4096
	.globl	_start
	.type	_start,@function
_start:
	cmp	%ebx, %eax
	jae	1f
	movl	foo@indntpoff, %eax
	movl	%gs:(%eax), %eax
1:	ret
