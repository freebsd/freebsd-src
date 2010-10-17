	.section ".tdata", #alloc, #write, #tls
	.align	4
	.globl sG1, sG2, sG3, sG4, sG5, sG6, sG7, sG8
sG1:	.word 513
sG2:	.word 514
sG3:	.word 515
sG4:	.word 516
sG5:	.word 517
sG6:	.word 518
sG7:	.word 519
sG8:	.word 520

	.text
	/* Dummy.  */
	.globl	__tls_get_addr
	.type   __tls_get_addr,#function
	.proc	04
__tls_get_addr:
	ret
	restore
