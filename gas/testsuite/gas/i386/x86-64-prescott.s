#Prescott New Instructions

	.text
foo:
	addsubpd	(%rcx),%xmm0
	addsubpd	%xmm2,%xmm1
	addsubps	(%rbx),%xmm2
	addsubps	%xmm4,%xmm3
	fisttp		0x90909090(%rax)
	fisttpl		0x90909090(%rax)
	fisttpll	0x90909090(%rax)
	haddpd		0x0(%rbp),%xmm4
	haddpd		%xmm6,%xmm5
	haddps		(%rdi),%xmm6
	haddps		%xmm0,%xmm7
	hsubpd		%xmm1,%xmm0
	hsubpd		(%rdx),%xmm1
	hsubps		%xmm2,%xmm2
	hsubps		(%rsp,1),%xmm3
	lddqu		(%rsi),%xmm5
	monitor
	monitor		%rax,%rcx,%rdx
	movddup		%xmm7,%xmm6
	movddup		(%rax),%xmm7
	movshdup	(%rcx),%xmm0
	movshdup	%xmm2,%xmm1
	movsldup	(%rbx),%xmm2
	movsldup	%xmm4,%xmm3
	mwait
	mwait		%rax,%rcx

	monitor		%eax,%rcx,%rdx
	addr32 monitor

	.p2align	4,0
