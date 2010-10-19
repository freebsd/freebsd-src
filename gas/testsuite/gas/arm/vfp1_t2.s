@ VFP Instructions for D variants (Double precision)
@ Same as vfp1.s, but for Thumb-2
	.syntax unified
	.thumb
	.text
	.global F
F:
	@ First we test the basic syntax and bit patterns of the opcodes.
	@ Most of these tests deliberatly use d0/r0 to avoid setting
	@ any more bits than necessary.

	@ Comparison operations

	fcmped	d0, d0
	fcmpezd	d0
	fcmpd	d0, d0
	fcmpzd	d0

	@ Monadic data operations

	fabsd	d0, d0
	fcpyd	d0, d0
	fnegd	d0, d0
	fsqrtd	d0, d0

	@ Dyadic data operations

	faddd	d0, d0, d0
	fdivd	d0, d0, d0
	fmacd	d0, d0, d0
	fmscd	d0, d0, d0
	fmuld	d0, d0, d0
	fnmacd	d0, d0, d0
	fnmscd	d0, d0, d0
	fnmuld	d0, d0, d0
	fsubd	d0, d0, d0

	@ Load/store operations

	fldd	d0, [r0]
	fstd	d0, [r0]

	@ Load/store multiple operations

	fldmiad	r0, {d0}
	fldmfdd	r0, {d0}
	fldmiad	r0!, {d0}
	fldmfdd	r0!, {d0}
	fldmdbd	r0!, {d0}
	fldmead	r0!, {d0}

	fstmiad	r0, {d0}
	fstmead	r0, {d0}
	fstmiad	r0!, {d0}
	fstmead	r0!, {d0}
	fstmdbd	r0!, {d0}
	fstmfdd	r0!, {d0}

	@ Conversion operations

	fsitod	d0, s0
	fuitod	d0, s0

	ftosid	s0, d0
	ftosizd	s0, d0
	ftouid	s0, d0
	ftouizd	s0, d0

	fcvtds	d0, s0
	fcvtsd	s0, d0
	
	@ ARM from VFP operations

	fmrdh	r0, d0
	fmrdl	r0, d0

	@ VFP From ARM operations

	fmdhr	d0, r0
	fmdlr	d0, r0

	@ Now we test that the register fields are updated correctly for
	@ each class of instruction.

	@ Single register operations (compare-zero):

	fcmpzd	d1
	fcmpzd	d2
	fcmpzd	d15

	@ Two register comparison operations:

	fcmpd	d0, d1
	fcmpd	d0, d2
	fcmpd	d0, d15
	fcmpd	d1, d0
	fcmpd	d2, d0
	fcmpd	d15, d0
	fcmpd	d5, d12

	@ Two register data operations (monadic)

	fnegd	d0, d1
	fnegd	d0, d2
	fnegd	d0, d15
	fnegd	d1, d0
	fnegd	d2, d0
	fnegd	d15, d0
	fnegd	d12, d5
	
	@ Three register data operations (dyadic)

	faddd	d0, d0, d1
	faddd	d0, d0, d2
	faddd	d0, d0, d15
	faddd	d0, d1, d0
	faddd	d0, d2, d0
	faddd	d0, d15, d0
	faddd	d1, d0, d0
	faddd	d2, d0, d0
	faddd	d15, d0, d0
	faddd	d12, d9, d5

	@ Conversion operations

	fcvtds	d0, s1
	fcvtds	d0, s2
	fcvtds	d0, s31
	fcvtds	d1, s0
	fcvtds	d2, s0
	fcvtds	d15, s0
	fcvtsd	s1, d0
	fcvtsd	s2, d0
	fcvtsd	s31, d0
	fcvtsd	s0, d1
	fcvtsd	s0, d2
	fcvtsd	s0, d15

	@ Move to VFP from ARM

	fmrdh	r1, d0
	fmrdh	r14, d0
	fmrdh	r0, d1
	fmrdh	r0, d2
	fmrdh	r0, d15
	fmrdl	r1, d0
	fmrdl	r14, d0
	fmrdl	r0, d1
	fmrdl	r0, d2
	fmrdl	r0, d15

	@ Move to ARM from VFP

	fmdhr	d0, r1
	fmdhr	d0, r14
	fmdhr	d1, r0
	fmdhr	d2, r0
	fmdhr	d15, r0
	fmdlr	d0, r1
	fmdlr	d0, r14
	fmdlr	d1, r0
	fmdlr	d2, r0
	fmdlr	d15, r0

	@ Load/store operations

	fldd	d0, [r1]
	fldd	d0, [r14]
	fldd	d0, [r0, #0]
	fldd	d0, [r0, #1020]
	fldd	d0, [r0, #-1020]
	fldd	d1, [r0]
	fldd	d2, [r0]
	fldd	d15, [r0]
	fstd	d12, [r12, #804]

	@ Load/store multiple operations

	fldmiad	r0, {d1}
	fldmiad	r0, {d2}
	fldmiad	r0, {d15}
	fldmiad	r0, {d0-d1}
	fldmiad	r0, {d0-d2}
	fldmiad	r0, {d0-d15}
	fldmiad	r0, {d1-d15}
	fldmiad	r0, {d2-d15}
	fldmiad	r0, {d14-d15}
	fldmiad	r1, {d0}
	fldmiad	r14, {d0}

	@ Check that we assemble all the register names correctly

	fcmpzd	d0
	fcmpzd	d1
	fcmpzd	d2
	fcmpzd	d3
	fcmpzd	d4
	fcmpzd	d5
	fcmpzd	d6
	fcmpzd	d7
	fcmpzd	d8
	fcmpzd	d9
	fcmpzd	d10
	fcmpzd	d11
	fcmpzd	d12
	fcmpzd	d13
	fcmpzd	d14
	fcmpzd	d15

	@ Now we check the placement of the conditional execution substring.
	@ On VFP this is always at the end of the instruction.
	
	@ Comparison operations

	itttt eq
	fcmpedeq	d1, d15
	fcmpezdeq	d2
	fcmpdeq	d3, d14
	fcmpzdeq	d4

	@ Monadic data operations

	itttt eq
	fabsdeq	d5, d13
	fcpydeq	d6, d12
	fnegdeq	d7, d11
	fsqrtdeq	d8, d10

	@ Dyadic data operations

	itttt eq
	fadddeq	d9, d1, d15
	fdivdeq	d2, d3, d14
	fmacdeq	d4, d13, d12
	fmscdeq	d5, d6, d11
	itttt eq
	fmuldeq	d7, d10, d9
	fnmacdeq	d8, d9, d10
	fnmscdeq	d7, d6, d11
	fnmuldeq	d5, d4, d12
	ittt eq
	fsubdeq	d3, d13, d14

	@ Load/store operations

	flddeq	d2, [r5]
	fstdeq	d1, [r12]

	@ Load/store multiple operations

	itttt eq
	fldmiadeq	r1, {d1}
	fldmfddeq	r2, {d2}
	fldmiadeq	r3!, {d3}
	fldmfddeq	r4!, {d4}
	itttt eq
	fldmdbdeq	r5!, {d5}
	fldmeadeq	r6!, {d6}

	fstmiadeq	r7, {d15}
	fstmeadeq	r8, {d14}
	itttt eq
	fstmiadeq	r9!, {d13}
	fstmeadeq	r10!, {d12}
	fstmdbdeq	r11!, {d11}
	fstmfddeq	r12!, {d10}

	@ Conversion operations

	itttt eq
	fsitodeq	d15, s1
	fuitodeq	d1, s31

	ftosideq	s1, d15
	ftosizdeq	s31, d2
	itttt eq
	ftouideq	s15, d2
	ftouizdeq	s11, d3

	fcvtdseq	d1, s10
	fcvtsdeq	s11, d1
	
	@ ARM from VFP operations

	itttt eq
	fmrdheq	r8, d1
	fmrdleq	r7, d15

	@ VFP From ARM operations

	fmdhreq	d1, r15
	fmdlreq	d15, r1

	# Add three nop instructions to ensure that the
	# output is 32-byte aligned as required for arm-aout.
	nop
	nop
	nop
