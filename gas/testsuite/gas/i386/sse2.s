foo:
	movnti		%eax, (%eax)
	sfence
	lfence
	mfence
	addpd		(%ecx),%xmm0
	addpd		%xmm2,%xmm1
	addsd		(%ebx),%xmm2
	addsd		%xmm4,%xmm3
	andnpd		0x0(%ebp),%xmm4
	andnpd		%xmm6,%xmm5
	andpd		(%edi),%xmm6
	andpd		%xmm0,%xmm7
	cmppd		$0x2,%xmm1,%xmm0
	cmppd		$0x3,(%edx),%xmm1
	cmpsd		$0x4,%xmm2,%xmm2
	cmpsd		$0x5,(%esp,1),%xmm3
	cmppd		$0x6,%xmm5,%xmm4
	cmppd		$0x7,(%esi),%xmm5
	cmpsd		$0x0,%xmm7,%xmm6
	cmpsd		$0x1,(%eax),%xmm7
	cmpeqpd		%xmm1,%xmm0
	cmpeqpd		(%edx),%xmm1
	cmpeqsd		%xmm2,%xmm2
	cmpeqsd		(%esp,1),%xmm3
	cmpltpd		%xmm5,%xmm4
	cmpltpd		(%esi),%xmm5
	cmpltsd		%xmm7,%xmm6
	cmpltsd		(%eax),%xmm7
	cmplepd		(%ecx),%xmm0
	cmplepd		%xmm2,%xmm1
	cmplesd		(%ebx),%xmm2
	cmplesd		%xmm4,%xmm3
	cmpunordpd	0x0(%ebp),%xmm4
	cmpunordpd	%xmm6,%xmm5
	cmpunordsd	(%edi),%xmm6
	cmpunordsd	%xmm0,%xmm7
	cmpneqpd	%xmm1,%xmm0
	cmpneqpd	(%edx),%xmm1
	cmpneqsd	%xmm2,%xmm2
	cmpneqsd	(%esp,1),%xmm3
	cmpnltpd	%xmm5,%xmm4
	cmpnltpd	(%esi),%xmm5
	cmpnltsd	%xmm7,%xmm6
	cmpnltsd	(%eax),%xmm7
	cmpnlepd	(%ecx),%xmm0
	cmpnlepd	%xmm2,%xmm1
	cmpnlesd	(%ebx),%xmm2
	cmpnlesd	%xmm4,%xmm3
	cmpordpd	0x0(%ebp),%xmm4
	cmpordpd	%xmm6,%xmm5
	cmpordsd	(%edi),%xmm6
	cmpordsd	%xmm0,%xmm7
	comisd		%xmm1,%xmm0
	comisd		(%edx),%xmm1
	cvtpi2pd	%mm3,%xmm2
	cvtpi2pd	(%esp,1),%xmm3
	cvtsi2sd	%ebp,%xmm4
	cvtsi2sd	(%esi),%xmm5
	cvtpd2pi	%xmm7,%mm6
	cvtpd2pi	(%eax),%mm7
	cvtsd2si	(%ecx),%eax
	cvtsd2si	%xmm2,%ecx
	cvttpd2pi	(%ebx),%mm2
	cvttpd2pi	%xmm4,%mm3
	cvttsd2si	0x0(%ebp),%esp
	cvttsd2si	%xmm6,%ebp
	divpd		%xmm1,%xmm0
	divpd		(%edx),%xmm1
	divsd		%xmm3,%xmm2
	divsd		(%esp,1),%xmm3
	ldmxcsr		0x0(%ebp)
	stmxcsr		(%esi)
	sfence
	maxpd		%xmm1,%xmm0
	maxpd		(%edx),%xmm1
	maxsd		%xmm3,%xmm2
	maxsd		(%esp,1),%xmm3
	minpd		%xmm5,%xmm4
	minpd		(%esi),%xmm5
	minsd		%xmm7,%xmm6
	minsd		(%eax),%xmm7
	movapd		%xmm1,%xmm0
	movapd		%xmm2,(%ecx)
	movapd		(%edx),%xmm2
	movhpd		%xmm5,(%esp,1)
	movhpd		(%esi),%xmm5
	movlpd		%xmm0,(%edi)
	movlpd		(%eax),%xmm0
	movmskpd	%xmm2,%ecx
	movupd		%xmm3,%xmm2
	movupd		%xmm4,(%edx)
	movupd		0x0(%ebp),%xmm4
	movsd		%xmm6,%xmm5
	movsd		%xmm7,(%esi)
	movsd		(%eax),%xmm7
	mulpd		%xmm1,%xmm0
	mulpd		(%edx),%xmm1
	mulsd		%xmm2,%xmm2
	mulsd		(%esp,1),%xmm3
	orpd		%xmm5,%xmm4
	orpd		(%esi),%xmm5
	shufpd		$0x2,(%edi),%xmm6
	shufpd		$0x3,%xmm0,%xmm7
	sqrtpd		%xmm1,%xmm0
	sqrtpd		(%edx),%xmm1
	sqrtsd		%xmm2,%xmm2
	sqrtsd		(%esp,1),%xmm3
	subpd		%xmm5,%xmm4
	subpd		(%esi),%xmm5
	subsd		%xmm7,%xmm6
	subsd		(%eax),%xmm7
	ucomisd		(%ecx),%xmm0
	ucomisd		%xmm2,%xmm1
	unpckhpd	(%ebx),%xmm2
	unpckhpd	%xmm4,%xmm3
	unpcklpd	0x0(%ebp),%xmm4
	unpcklpd	%xmm6,%xmm5
	xorpd		(%edi),%xmm6
	xorpd		%xmm0,%xmm7
	movntpd		%xmm6,(%ebx)
	xorpd		%xmm0, %xmm1
	cvtdq2pd	%xmm0, %xmm1
	cvtpd2dq	%xmm0, %xmm1
	cvtdq2ps	%xmm0, %xmm1
	cvtpd2ps	%xmm0, %xmm1
	cvtps2pd	%xmm0, %xmm1
	cvtps2dq	%xmm0, %xmm1
	cvtsd2ss	%xmm0, %xmm1
	cvtss2sd	%xmm0, %xmm1
	cvttpd2dq	%xmm0, %xmm1
	cvttps2dq	%xmm0, %xmm1
	maskmovdqu	%xmm0, %xmm1
	movdqa		%xmm0, %xmm1
	movdqa		%xmm0, (%esi)
	movdqu		%xmm0, %xmm1
	movdqu		%xmm0, (%esi)
	movdq2q		%xmm0, %mm1
	movq2dq		%mm0, %xmm1
	pmuludq		%xmm0, %xmm1
	pmuludq		%xmm0, %xmm1
	pshufd		$1, %xmm0, %xmm1
	pshufhw		$1, %xmm0, %xmm1
	pshuflw		$1, %xmm0, %xmm1
	pslldq		$1, %xmm0
	psrldq		$1, %xmm0
	punpckhqdq	%xmm0, %xmm1

 .p2align 4
