	.text
	.global start

!	Starting point
start:

!	test all ASRs

	rd	%asr0, %l0
	rd	%asr1, %l0
	rd	%asr15, %l0
	rd	%asr17, %l0
	rd	%asr18, %l0
	rd	%asr19, %l0	! should stop the processor
	rd	%asr20, %l0
	rd	%asr21, %l0
	rd	%asr22, %l0

	wr	%l0, 0, %asr0
	wr	%l0, 0, %asr1
	wr	%l0, 0, %asr15
	wr	%l0, 0, %asr17
	wr	%l0, 0, %asr18
	wr	%l0, 0, %asr19
	wr	%l0, 0, %asr20
	wr	%l0, 0, %asr21
	wr	%l0, 0, %asr22

!	test UMUL with no overflow inside Y
test_umul:
	umul	%g1, %g2, %g3

!	test UMUL with an overflow inside Y

	umul	%g1, %g2, %g3	! %g3 must be equal to 0

!	test SMUL with negative result
test_smul:
	smul	%g1, %g2, %g3

!	test SMUL with positive result

	smul	%g1, %g2, %g3

!	test STBAR: there are two possible syntaxes
test_stbar:
	stbar			! is a valid V8 syntax, at least a synthetic
				! instruction
	rd	%asr15, %g0	! other solution

!	test UNIMP
	unimp	1

!	test FLUSH
	flush	%l1		! is the official V8 syntax

!	test SCAN: find first 0
test_scan:
	scan	%l1, 0xffffffff, %l3

!	test scan: find first 1

	scan	%l1, 0, %l3

!	test scan: find first bit != bit-0

	scan	%l1, %l1, %l3

!	test SHUFFLE
test_shuffle:
	shuffle	%l0, 0x1, %l1
	shuffle	%l0, 0x2, %l1
	shuffle	%l0, 0x4, %l1
	shuffle	%l0, 0x8, %l1
	shuffle	%l0, 0x10, %l1
	shuffle	%l0, 0x18, %l1

!	test UMAC
test_umac:
	umac	%l1, %l2, %l0
	umac	%l1, 2, %l0
	umac	2, %l1, %l0

!	test UMACD
test_umacd:
	umacd	%l2, %l4, %l0
	umacd	%l2, 3, %l0
	umacd	3, %l2, %l0

!	test SMAC
test_smac:
	smac	%l1, %l2, %l0
	smac	%l1, -42, %l0
	smac	-42, %l1, %l0

!	test SMACD
test_smacd:
	smacd	%l2, %l4, %l0
	smacd	%l2, 123, %l0
	smacd	123, %l2, %l0

!	test UMULD
test_umuld:
	umuld	%o2, %o4, %o0
	umuld	%o2, 0x234, %o0
	umuld	0x567, %o2, %o0

!	test SMULD
test_smuld:
	smuld	%i2, %i4, %i0
	smuld	%i2, -4096, %i0
	smuld	4095, %i4, %i0

!	Coprocessor instructions
test_coprocessor:
!	%ccsr	is register # 0
!	%ccfr	is register # 1
!	%ccpr	is register # 3
!	%cccrcr is register # 2

!	test CPUSH: just syntax

	cpush	%l0, %l1
	cpush	%l0, 1
	cpusha	%l0, %l1
	cpusha	%l0, 1

!	test CPULL: just syntax

	cpull	%l0

!	test CPRDCXT: just syntax

	crdcxt	%ccsr, %l0
	crdcxt	%ccfr, %l0
	crdcxt	%ccpr, %l0
	crdcxt	%cccrcr, %l0

!	test CPWRCXT: just syntax

	cwrcxt	%l0, %ccsr
	cwrcxt	%l0, %ccfr
	cwrcxt	%l0, %ccpr
	cwrcxt	%l0, %cccrcr

!	test CBccc: just syntax

	cbn	stop
	nop
	cbn,a	stop
	nop
	cbe	stop
	nop
	cbe,a	stop
	nop
	cbf	stop
	nop
	cbf,a	stop
	nop
	cbef	stop
	nop
	cbef,a	stop
	nop
	cbr	stop
	nop
	cbr,a	stop
	nop
	cber	stop
	nop
	cber,a	stop
	nop
	cbfr	stop
	nop
	cbfr,a	stop
	nop
	cbefr	stop
	nop
	cbefr,a	stop
	nop
	cba	stop
	nop
	cba,a	stop
	nop
	cbne	stop
	nop
	cbne,a	stop
	nop
	cbnf	stop
	nop
	cbnf,a	stop
	nop
	cbnef	stop
	nop
	cbnef,a	stop
	nop
	cbnr	stop
	nop
	cbnr,a	stop
	nop
	cbner	stop
	nop
	cbner,a	stop
	nop
	cbnfr	stop
	nop
	cbnfr,a	stop
	nop
	cbnefr	stop
	nop
	cbnefr,a	stop
	nop
