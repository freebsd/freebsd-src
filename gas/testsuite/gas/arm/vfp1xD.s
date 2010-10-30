@ VFP Instructions for v1xD variants (Single precision only)
	.text
	.global F
F:
	@ First we test the basic syntax and bit patterns of the opcodes.
	@ Most of these tests deliberatly use s0/r0 to avoid setting
	@ any more bits than necessary.

	@ Comparison operations

	fmstat

	fcmpes	s0, s0
	fcmpezs	s0
	fcmps	s0, s0
	fcmpzs	s0

	@ Monadic data operations

	fabss	s0, s0
	fcpys	s0, s0
	fnegs	s0, s0
	fsqrts	s0, s0

	@ Dyadic data operations

	fadds	s0, s0, s0
	fdivs	s0, s0, s0
	fmacs	s0, s0, s0
	fmscs	s0, s0, s0
	fmuls	s0, s0, s0
	fnmacs	s0, s0, s0
	fnmscs	s0, s0, s0
	fnmuls	s0, s0, s0
	fsubs	s0, s0, s0

	@ Load/store operations

	flds	s0, [r0]
	fsts	s0, [r0]

	@ Load/store multiple operations

	fldmias	r0, {s0}
	fldmfds	r0, {s0}
	fldmias	r0!, {s0}
	fldmfds	r0!, {s0}
	fldmdbs	r0!, {s0}
	fldmeas	r0!, {s0}

	fldmiax	r0, {d0}
	fldmfdx	r0, {d0}
	fldmiax	r0!, {d0}
	fldmfdx	r0!, {d0}
	fldmdbx	r0!, {d0}
	fldmeax	r0!, {d0}

	fstmias	r0, {s0}
	fstmeas	r0, {s0}
	fstmias	r0!, {s0}
	fstmeas	r0!, {s0}
	fstmdbs	r0!, {s0}
	fstmfds	r0!, {s0}

	fstmiax	r0, {d0}
	fstmeax	r0, {d0}
	fstmiax	r0!, {d0}
	fstmeax	r0!, {d0}
	fstmdbx	r0!, {d0}
	fstmfdx	r0!, {d0}

	@ Conversion operations

	fsitos	s0, s0
	fuitos	s0, s0

	ftosis	s0, s0
	ftosizs	s0, s0
	ftouis	s0, s0
	ftouizs	s0, s0

	@ ARM from VFP operations

	fmrs	r0, s0
	fmrx	r0, fpsid
	fmrx	r0, fpscr
	fmrx	r0, fpexc

	@ VFP From ARM operations

	fmsr	s0, r0
	fmxr	fpsid, r0
	fmxr	fpscr, r0
	fmxr	fpexc, r0

	@ Now we test that the register fields are updated correctly for
	@ each class of instruction.

	@ Single register operations (compare-zero):

	fcmpzs	s1
	fcmpzs	s2
	fcmpzs	s31

	@ Two register comparison operations:

	fcmps	s0, s1
	fcmps	s0, s2
	fcmps	s0, s31
	fcmps	s1, s0
	fcmps	s2, s0
	fcmps	s31, s0
	fcmps	s21, s12

	@ Two register data operations (monadic)

	fnegs	s0, s1
	fnegs	s0, s2
	fnegs	s0, s31
	fnegs	s1, s0
	fnegs	s2, s0
	fnegs	s31, s0
	fnegs	s12, s21
	
	@ Three register data operations (dyadic)

	fadds	s0, s0, s1
	fadds	s0, s0, s2
	fadds	s0, s0, s31
	fadds	s0, s1, s0
	fadds	s0, s2, s0
	fadds	s0, s31, s0
	fadds	s1, s0, s0
	fadds	s2, s0, s0
	fadds	s31, s0, s0
	fadds	s12, s21, s5

	@ Conversion operations

	fsitos	s0, s1
	fsitos	s0, s2
	fsitos	s0, s31
	fsitos	s1, s0
	fsitos	s2, s0
	fsitos	s31, s0

	ftosis	s0, s1
	ftosis	s0, s2
	ftosis	s0, s31
	ftosis	s1, s0
	ftosis	s2, s0
	ftosis	s31, s0

	@ Move to VFP from ARM

	fmsr	s0, r1
	fmsr	s0, r7
	fmsr	s0, r14
	fmsr	s1, r0
	fmsr	s2, r0
	fmsr	s31, r0
	fmsr	s21, r7

	fmxr	fpsid, r1
	fmxr	fpsid, r14

	@ Move to ARM from VFP

	fmrs	r0, s1
	fmrs	r0, s2
	fmrs	r0, s31
	fmrs	r1, s0
	fmrs	r7, s0
	fmrs	r14, s0
	fmrs	r9, s11

	fmrx	r1, fpsid
	fmrx	r14, fpsid

	@ Load/store operations

	flds	s0, [r1]
	flds	s0, [r14]
	flds	s0, [r0, #0]
	flds	s0, [r0, #1020]
	flds	s0, [r0, #-1020]
	flds	s1, [r0]
	flds	s2, [r0]
	flds	s31, [r0]
	fsts	s21, [r12, #804]

	@ Load/store multiple operations

	fldmias	r0, {s1}
	fldmias	r0, {s2}
	fldmias	r0, {s31}
	fldmias	r0, {s0-s1}
	fldmias	r0, {s0-s2}
	fldmias	r0, {s0-s31}
	fldmias	r0, {s1-s31}
	fldmias	r0, {s2-s31}
	fldmias	r0, {s30-s31}
	fldmias	r1, {s0}
	fldmias	r14, {s0}

	fstmiax	r0, {d1}
	fstmiax	r0, {d2}
	fstmiax	r0, {d15}
	fstmiax	r0, {d0-d1}
	fstmiax	r0, {d0-d2}
	fstmiax	r0, {d0-d15}
	fstmiax	r0, {d1-d15}
	fstmiax	r0, {d2-d15}
	fstmiax	r0, {d14-d15}
	fstmiax	r1, {d0}
	fstmiax	r14, {d0}

	@ Check that we assemble all the register names correctly

	fcmpzs	s0
	fcmpzs	s1
	fcmpzs	s2
	fcmpzs	s3
	fcmpzs	s4
	fcmpzs	s5
	fcmpzs	s6
	fcmpzs	s7
	fcmpzs	s8
	fcmpzs	s9
	fcmpzs	s10
	fcmpzs	s11
	fcmpzs	s12
	fcmpzs	s13
	fcmpzs	s14
	fcmpzs	s15
	fcmpzs	s16
	fcmpzs	s17
	fcmpzs	s18
	fcmpzs	s19
	fcmpzs	s20
	fcmpzs	s21
	fcmpzs	s22
	fcmpzs	s23
	fcmpzs	s24
	fcmpzs	s25
	fcmpzs	s26
	fcmpzs	s27
	fcmpzs	s28
	fcmpzs	s29
	fcmpzs	s30
	fcmpzs	s31

	@ Now we check the placement of the conditional execution substring.
	@ On VFP this is always at the end of the instruction.
	@ We use different register numbers here to check for correct
	@ disassembly
	
	@ Comparison operations

	fmstateq

	fcmpeseq	s3, s7
	fcmpezseq	s5
	fcmpseq	s1, s2
	fcmpzseq	s1

	@ Monadic data operations

	fabsseq	s1, s3
	fcpyseq	s31, s19
	fnegseq	s20, s8
	fsqrtseq	s5, s7

	@ Dyadic data operations

	faddseq	s6, s5, s4
	fdivseq	s3, s2, s1
	fmacseq	s31, s30, s29
	fmscseq	s28, s27, s26
	fmulseq	s25, s24, s23
	fnmacseq	s22, s21, s20
	fnmscseq	s19, s18, s17
	fnmulseq	s16, s15, s14
	fsubseq	s13, s12, s11

	@ Load/store operations

	fldseq	s10, [r8]
	fstseq	s9, [r7]

	@ Load/store multiple operations

	fldmiaseq	r1, {s8}
	fldmfdseq	r2, {s7}
	fldmiaseq	r3!, {s6}
	fldmfdseq	r4!, {s5}
	fldmdbseq	r5!, {s4}
	fldmeaseq	r6!, {s3}

	fldmiaxeq	r7, {d1}
	fldmfdxeq	r8, {d2}
	fldmiaxeq	r9!, {d3}
	fldmfdxeq	r10!, {d4}
	fldmdbxeq	r11!, {d5}
	fldmeaxeq	r12!, {d6}

	fstmiaseq	r13, {s2}
	fstmeaseq	r14, {s1}
	fstmiaseq	r1!, {s31}
	fstmeaseq	r2!, {s30}
	fstmdbseq	r3!, {s29}
	fstmfdseq	r4!, {s28}

	fstmiaxeq	r5, {d7}
	fstmeaxeq	r6, {d8}
	fstmiaxeq	r7!, {d9}
	fstmeaxeq	r8!, {d10}
	fstmdbxeq	r9!, {d11}
	fstmfdxeq	r10!, {d12}

	@ Conversion operations

	fsitoseq	s27, s6
	ftosiseq	s25, s5
	ftosizseq	s23, s4
	ftouiseq	s21, s3
	ftouizseq	s19, s2
	fuitoseq	s17, s1

	@ ARM from VFP operations

	fmrseq	r11, s3
	fmrxeq	r9, fpsid

	@ VFP From ARM operations

	fmsreq	s3, r9
	fmxreq	fpsid, r8

	@ Implementation specific system registers
	fmrx	r0, fpinst
	fmrx	r0, fpinst2
	fmrx	r0, mvfr0
	fmrx	r0, mvfr1
	fmrx	r0, c12
	fmxr	fpinst, r0
	fmxr	fpinst2, r0
	fmxr	mvfr0, r0
	fmxr	mvfr1, r0
	fmxr	c12, r0

	nop
	nop
