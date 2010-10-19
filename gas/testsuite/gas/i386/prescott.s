#Prescott New Instructions

	.text
foo:
	addsubpd	(%ecx),%xmm0
	addsubpd	%xmm2,%xmm1
	addsubps	(%ebx),%xmm2
	addsubps	%xmm4,%xmm3
	fisttp		0x90909090(%eax)
	fisttpl		0x90909090(%eax)
	fisttpll	0x90909090(%eax)
	haddpd		0x0(%ebp),%xmm4
	haddpd		%xmm6,%xmm5
	haddps		(%edi),%xmm6
	haddps		%xmm0,%xmm7
	hsubpd		%xmm1,%xmm0
	hsubpd		(%edx),%xmm1
	hsubps		%xmm2,%xmm2
	hsubps		(%esp,1),%xmm3
	lddqu		(%esi),%xmm5
	monitor
	monitor		%eax,%ecx,%edx
	movddup		%xmm7,%xmm6
	movddup		(%eax),%xmm7
	movshdup	(%ecx),%xmm0
	movshdup	%xmm2,%xmm1
	movsldup	(%ebx),%xmm2
	movsldup	%xmm4,%xmm3
	mwait
	mwait		%eax,%ecx

	monitor		%ax,%ecx,%edx
	addr16 monitor

	.p2align	4,0
