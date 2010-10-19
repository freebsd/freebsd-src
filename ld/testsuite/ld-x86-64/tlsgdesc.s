	.text
	.globl	fc1
	.type	fc1,@function
fc1:
	pushq	%rbp
	movq	%rsp, %rbp
	nop;nop;nop;nop

	/* IE against global var.  */
	movq	%fs:0, %rcx
	nop;nop
	addq	sG3@gottpoff(%rip), %rcx
	nop;nop;nop;nop

	/* IE against global var.  */
	movq	%fs:0, %rcx
	nop;nop
	addq	sG4@gottpoff(%rip), %rcx
	nop;nop;nop;nop

	/* GD, gd first.  */
	.byte	0x66
	leaq	sG1@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	leaq	sG1@tlsdesc(%rip), %rax
	call	*sG1@tlscall(%rax)
	nop;nop;nop;nop

	/* GD, desc first.  */
	leaq	sG2@tlsdesc(%rip), %rax
	call	*sG2@tlscall(%rax)
	nop;nop;nop;nop

	.byte	0x66
	leaq	sG2@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE, gd first, after IE use.  */
	.byte	0x66
	leaq	sG3@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	leaq	sG3@tlsdesc(%rip), %rax
	call	*sG3@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> IE, desc first, after IE use.  */
	leaq	sG4@tlsdesc(%rip), %rax
	call	*sG4@tlscall(%rax)
	nop;nop;nop;nop

	.byte	0x66
	leaq	sG4@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	/* GD -> IE, gd first, before IE use.  */
	.byte	0x66
	leaq	sG5@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	leaq	sG5@tlsdesc(%rip), %rax
	call	*sG5@tlscall(%rax)
	nop;nop;nop;nop

	/* GD -> IE, desc first, before IE use.  */
	leaq	sG6@tlsdesc(%rip), %rax
	call	*sG6@tlscall(%rax)
	nop;nop;nop;nop

	.byte	0x66
	leaq	sG6@tlsgd(%rip), %rdi
	.word	0x6666
	rex64
	call	__tls_get_addr@plt
	nop;nop;nop;nop

	/* IE against global var.  */
	movq	%fs:0, %rcx
	nop;nop
	addq	sG5@gottpoff(%rip), %rcx
	nop;nop;nop;nop

	/* IE against global var.  */
	movq	%fs:0, %rcx
	nop;nop
	addq	sG6@gottpoff(%rip), %rcx
	nop;nop;nop;nop

	leave
	ret
