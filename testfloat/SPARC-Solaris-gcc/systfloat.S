
/*
===============================================================================

This GNU assembler source file is part of TestFloat, Release 2a, a package
of programs for testing the correctness of floating-point arithmetic
complying to the IEC/IEEE Standard for Floating-Point.

Written by John R. Hauser.  More information is available through the Web
page `http://HTTP.CS.Berkeley.EDU/~jhauser/arithmetic/TestFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort
has been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT
TIMES RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO
PERSONS AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ANY
AND ALL LOSSES, COSTS, OR OTHER PROBLEMS ARISING FROM ITS USE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) they include prominent notice that the work is derivative, and (2) they
include prominent notice akin to these four paragraphs for those parts of
this code that are retained.

===============================================================================
*/

	.text

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_int32_to_float32
syst_int32_to_float32:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fitos %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_int32_to_float64
syst_int32_to_float64:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fitod %f0,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_int32_to_float128
syst_int32_to_float128:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fitoq %f0,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_to_int32_round_to_zero
syst_float32_to_int32_round_to_zero:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fstoi %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_to_float64
syst_float32_to_float64:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fstod %f0,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_to_float128
syst_float32_to_float128:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fstoq %f0,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_add
syst_float32_add:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fadds %f0,%f1,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_sub
syst_float32_sub:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fsubs %f0,%f1,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_mul
syst_float32_mul:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fmuls %f0,%f1,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_div
syst_float32_div:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fdivs %f0,%f1,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_sqrt
syst_float32_sqrt:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	ld [%sp+96],%f0
	fsqrts %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_eq
syst_float32_eq:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmps %f0,%f1
	mov 0,%i0
	fbe,a float32EqExit
	mov 1,%i0
float32EqExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_le
syst_float32_le:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmpes %f0,%f1
	mov 0,%i0
	fble,a float32LeExit
	mov 1,%i0
float32LeExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_lt
syst_float32_lt:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmpes %f0,%f1
	mov 0,%i0
	fbl,a float32LtExit
	mov 1,%i0
float32LtExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_eq_signaling
syst_float32_eq_signaling:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmpes %f0,%f1
	mov 0,%i0
	fbe,a float32EqSignalingExit
	mov 1,%i0
float32EqSignalingExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_le_quiet
syst_float32_le_quiet:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmps %f0,%f1
	mov 0,%i0
	fble,a float32LeQuietExit
	mov 1,%i0
float32LeQuietExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float32_lt_quiet
syst_float32_lt_quiet:
	save %sp,-128,%sp

	st %i0,[%sp+96]
	st %i1,[%sp+100]
	ld [%sp+96],%f0
	ld [%sp+100],%f1
	fcmps %f0,%f1
	mov 0,%i0
	fbl,a float32LtQuietExit
	mov 1,%i0
float32LtQuietExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_to_int32_round_to_zero
syst_float64_to_int32_round_to_zero:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	ldd [%sp+96],%f0
	fdtoi %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_to_float32
syst_float64_to_float32:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	ldd [%sp+96],%f0
	fdtos %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_to_float128
syst_float64_to_float128:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	ldd [%sp+96],%f0
	fdtoq %f0,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_add
syst_float64_add:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	faddd %f0,%f2,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_sub
syst_float64_sub:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fsubd %f0,%f2,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_mul
syst_float64_mul:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fmuld %f0,%f2,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_div
syst_float64_div:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fdivd %f0,%f2,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_sqrt
syst_float64_sqrt:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	ldd [%sp+96],%f0
	fsqrtd %f0,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_eq
syst_float64_eq:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmpd %f0,%f2
	mov 0,%i0
	fbe,a float64EqExit
	mov 1,%i0
float64EqExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_le
syst_float64_le:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmped %f0,%f2
	mov 0,%i0
	fble,a float64LeExit
	mov 1,%i0
float64LeExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_lt
syst_float64_lt:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmped %f0,%f2
	mov 0,%i0
	fbl,a float64LtExit
	mov 1,%i0
float64LtExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_eq_signaling
syst_float64_eq_signaling:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmped %f0,%f2
	mov 0,%i0
	fbe,a float64EqSignalingExit
	mov 1,%i0
float64EqSignalingExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_le_quiet
syst_float64_le_quiet:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmpd %f0,%f2
	mov 0,%i0
	fble,a float64LeQuietExit
	mov 1,%i0
float64LeQuietExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float64_lt_quiet
syst_float64_lt_quiet:
	save %sp,-128,%sp

	std %i0,[%sp+96]
	std %i2,[%sp+104]
	ldd [%sp+96],%f0
	ldd [%sp+104],%f2
	fcmpd %f0,%f2
	mov 0,%i0
	fbl,a float64LtQuietExit
	mov 1,%i0
float64LtQuietExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_to_int32_round_to_zero
syst_float128_to_int32_round_to_zero:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	fqtoi %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_to_float32
syst_float128_to_float32:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	fqtos %f0,%f0
	st %f0,[%sp+96]
	ld [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_to_float64
syst_float128_to_float64:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	fqtod %f0,%f0
	std %f0,[%sp+96]
	ldd [%sp+96],%i0

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_add
syst_float128_add:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	faddq %f0,%f4,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_sub
syst_float128_sub:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fsubq %f0,%f4,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_mul
syst_float128_mul:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fmulq %f0,%f4,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_div
syst_float128_div:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fdivq %f0,%f4,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_sqrt
syst_float128_sqrt:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	fsqrtq %f0,%f0
	ld [%sp+192],%o0
	std %f0,[%o0]
	std %f2,[%o0+8]

	jmp %i7+12
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_eq
syst_float128_eq:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpq %f0,%f4
	mov 0,%i0
	fbe,a float128EqExit
	mov 1,%i0
float128EqExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_le
syst_float128_le:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpeq %f0,%f4
	mov 0,%i0
	fble,a float128Le
	mov 1,%i0
float128Le:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_lt
syst_float128_lt:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpeq %f0,%f4
	mov 0,%i0
	fbl,a float128Lt
	mov 1,%i0
float128Lt:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_eq_signaling
syst_float128_eq_signaling:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpeq %f0,%f4
	mov 0,%i0
	fbe,a float128EqSignalingExit
	mov 1,%i0
float128EqSignalingExit:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_le_quiet
syst_float128_le_quiet:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpq %f0,%f4
	mov 0,%i0
	fble,a float128LeQuiet
	mov 1,%i0
float128LeQuiet:

	ret
	restore

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 4
	.global syst_float128_lt_quiet
syst_float128_lt_quiet:
	save %sp,-128,%sp

	ldd [%i0],%f0
	ldd [%i0+8],%f2
	ldd [%i1],%f4
	ldd [%i1+8],%f6
	fcmpq %f0,%f4
	mov 0,%i0
	fbl,a float128LtQuiet
	mov 1,%i0
float128LtQuiet:

	ret
	restore

