.text
	.type _start,@function
_start:

	pmpyshr2 r4 = r5, r6, 0
	pmpyshr2.u r4 = r5, r6, 16

	pmpy2.r r4 = r5, r6
	pmpy2.l r4 = r5, r6

	mix1.r r4 = r5, r6
	mix2.r r4 = r5, r6
	mix4.r r4 = r5, r6
	mix1.l r4 = r5, r6
	mix2.l r4 = r5, r6
	mix4.l r4 = r5, r6

	pack2.uss r4 = r5, r6
	pack2.sss r4 = r5, r6
	pack4.sss r4 = r5, r6

	unpack1.h r4 = r5, r6
	unpack2.h r4 = r5, r6
	unpack4.h r4 = r5, r6
	unpack1.l r4 = r5, r6
	unpack2.l r4 = r5, r6
	unpack4.l r4 = r5, r6

	pmin1.u r4 = r5, r6
	pmax1.u r4 = r5, r6

	pmin2 r4 = r5, r6
	pmax2 r4 = r5, r6

	psad1 r4 = r5, r6

	mux1 r4 = r5, @rev
	mux1 r4 = r5, @mix
	mux1 r4 = r5, @shuf
	mux1 r4 = r5, @alt
	mux1 r4 = r5, @brcst

	mux2 r4 = r5, 0
	mux2 r4 = r5, 0xff
	mux2 r4 = r5, 0xaa

	pshr2 r4 = r5, r6
	pshr2 r4 = r5, 0
	pshr2 r4 = r5, 8
	pshr2 r4 = r5, 31

	pshr4 r4 = r5, r6
	pshr4 r4 = r5, 0
	pshr4 r4 = r5, 8
	pshr4 r4 = r5, 31

	pshr2.u r4 = r5, r6
	pshr2.u r4 = r5, 0
	pshr2.u r4 = r5, 8
	pshr2.u r4 = r5, 31

	pshr4.u r4 = r5, r6
	pshr4.u r4 = r5, 0
	pshr4.u r4 = r5, 8
	pshr4.u r4 = r5, 31

	shr r4 = r5, r6
	shr.u r4 = r5, r6

	pshl2 r4 = r5, r6
	pshl2 r4 = r5, 0
	pshl2 r4 = r5, 8
	pshl2 r4 = r5, 31

	pshl4 r4 = r5, r6
	pshl4 r4 = r5, 0
	pshl4 r4 = r5, 8
	pshl4 r4 = r5, 31

	shl r4 = r5, r6

	popcnt r4 = r5

	shrp r4 = r5, r6, 0
	shrp r4 = r5, r6, 12
	shrp r4 = r5, r6, 63

	extr r4 = r5, 0, 16
	extr r4 = r5, 0, 63
	extr r4 = r5, 10, 40
	
	extr.u r4 = r5, 0, 16
	extr.u r4 = r5, 0, 63
	extr.u r4 = r5, 10, 40
	
	dep.z r4 = r5, 0, 16
	dep.z r4 = r5, 0, 63
	dep.z r4 = r5, 10, 40
	dep.z r4 = 0, 0, 16
	dep.z r4 = 127, 0, 63
	dep.z r4 = -128, 5, 50
	dep.z r4 = 0x55, 10, 40

	dep r4 = 0, r5, 0, 16
	dep r4 = -1, r5, 0, 63
// Insert padding NOPs to force the same template selection as IAS.
	nop.m 0
	nop.f 0
	dep r4 = r5, r6, 10, 7

	movl r4 = 0
	movl r4 = 0xffffffffffffffff
	movl r4 = 0x1234567890abcdef

	break.i 0
	break.i 0x1fffff

	nop.i 0
	nop.i 0x1fffff

	chk.s.i r4, _start

	mov r4 = b0
	mov b0 = r4

	mov pr = r4, 0
	mov pr = r4, 0x1234
	mov pr = r4, 0x1ffff

	mov pr.rot = 0
// ??? This was originally 0x3ffffff, but that generates an assembler warning
// that the testsuite infrastructure isn't set up to ignore.
	mov pr.rot = 0x3ff0000
	mov pr.rot = -0x4000000

	zxt1 r4 = r5
	zxt2 r4 = r5
	zxt4 r4 = r5

	sxt1 r4 = r5
	sxt2 r4 = r5
	sxt4 r4 = r5

	czx1.l r4 = r5
	czx2.l r4 = r5
	czx1.r r4 = r5
	czx2.r r4 = r5

	tbit.z p2, p3 = r4, 0
	tbit.z.unc p2, p3 = r4, 1
	tbit.z.and p2, p3 = r4, 2
	tbit.z.or p2, p3 = r4, 3
	tbit.z.or.andcm p2, p3 = r4, 4
	tbit.z.orcm p2, p3 = r4, 5
	tbit.z.andcm p2, p3 = r4, 6
	tbit.z.and.orcm p2, p3 = r4, 7
	tbit.nz p2, p3 = r4, 8
	tbit.nz.unc p2, p3 = r4, 9
	tbit.nz.and p2, p3 = r4, 10
	tbit.nz.or p2, p3 = r4, 11
	tbit.nz.or.andcm p2, p3 = r4, 12
	tbit.nz.orcm p2, p3 = r4, 13
	tbit.nz.andcm p2, p3 = r4, 14
	tbit.nz.and.orcm p2, p3 = r4, 15

	tnat.z p2, p3 = r4
	tnat.z.unc p2, p3 = r4
	tnat.z.and p2, p3 = r4
	tnat.z.or p2, p3 = r4
	tnat.z.or.andcm p2, p3 = r4
	tnat.z.orcm p2, p3 = r4
	tnat.z.andcm p2, p3 = r4
	tnat.z.and.orcm p2, p3 = r4
	tnat.nz p2, p3 = r4
	tnat.nz.unc p2, p3 = r4
	tnat.nz.and p2, p3 = r4
	tnat.nz.or p2, p3 = r4
	tnat.nz.or.andcm p2, p3 = r4
	tnat.nz.orcm p2, p3 = r4
	tnat.nz.andcm p2, p3 = r4
	tnat.nz.and.orcm p2, p3 = r4

	mov b3 = r4, .L1
	mov.imp b3 = r4, .L1
.space 240
.L1:
	mov.sptk b3 = r4, .L2
	mov.sptk.imp b3 = r4, .L2
.space 240
.L2:
	mov.dptk b3 = r4, .L3
	mov.dptk.imp b3 = r4, .L3
.space 240
.L3:

	mov.ret b3 = r4, .L4
	mov.ret.imp b3 = r4, .L4
.space 240
.L4:
	mov.ret.sptk b3 = r4, .L5
	mov.ret.sptk.imp b3 = r4, .L5
.space 240
.L5:
	mov.ret.dptk b3 = r4, .L6
	mov.ret.dptk.imp b3 = r4, .L6
.space 240
.L6:

	# instructions added by SDM2.1:

	hint @pause
	hint.i 0
	hint.i @pause
	hint.i 0x1fffff
(p7)	hint @pause
(p7)	hint.i 0
(p7)	hint.i @pause
(p7)	hint.i 0x1fffff
 (p7)	hint @pause
 (p7)	hint.i 0
 (p7)	hint.i @pause
 (p7)	hint.i 0x1fffff
