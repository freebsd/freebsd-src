	.section ".tdata", "awT", @progbits
	.globl sG1, sG2, sG3, sG4, sG5, sG6, sG7, sG8
sG1:	.long 513
sG2:	.long 514
sG3:	.long 515
sG4:	.long 516
sG5:	.long 517
sG6:	.long 518
sG7:	.long 519
sG8:	.long 520

	.text
	/* Dummy.  */
	.globl __tls_get_addr
	.type   __tls_get_addr,@function
__tls_get_addr:
	movq	%rdi, %rax
	ret
