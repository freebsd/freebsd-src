
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
	.align 2
.globl _syst_int32_to_floatx80
_syst_int32_to_floatx80:
	fildl 8(%esp)
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_int64_to_floatx80
_syst_int64_to_floatx80:
	fildq 8(%esp)
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_float32_to_floatx80
_syst_float32_to_floatx80:
	flds 8(%esp)
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_float64_to_floatx80
_syst_float64_to_floatx80:
	fldl 8(%esp)
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_to_int32
_syst_floatx80_to_int32:
	fldt 4(%esp)
	subl $4,%esp
	fistpl (%esp)
	movl (%esp),%eax
	addl $4,%esp
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_to_int64
_syst_floatx80_to_int64:
	fldt 4(%esp)
	subl $8,%esp
	fistpq (%esp)
	movl (%esp),%eax
	movl 4(%esp),%edx
	addl $8,%esp
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_to_float32
_syst_floatx80_to_float32:
	fldt 4(%esp)
	subl $4,%esp
	fstps (%esp)
	movl (%esp),%eax
	addl $4,%esp
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_to_float64
_syst_floatx80_to_float64:
	fldt 4(%esp)
	subl $8,%esp
	fstpl (%esp)
	movl 4(%esp),%edx
	movl (%esp),%eax
	addl $8,%esp
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_round_to_int
_syst_floatx80_round_to_int:
	fldt 8(%esp)
	frndint
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_add
_syst_floatx80_add:
	fldt 8(%esp)
	fldt 20(%esp)
	faddp
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_sub
_syst_floatx80_sub:
	fldt 8(%esp)
	fldt 20(%esp)
	fsubrp
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_mul
_syst_floatx80_mul:
	fldt 8(%esp)
	fldt 20(%esp)
	fmulp
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_div
_syst_floatx80_div:
	fldt 8(%esp)
	fldt 20(%esp)
	fdivrp
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_rem
_syst_floatx80_rem:
	fldt 20(%esp)
	fldt 8(%esp)
floatx80_rem_loop:
	fprem1
	fnstsw %ax
	btw $10,%ax
	jc floatx80_rem_loop
	movl 4(%esp),%eax
	fstpt (%eax)
	fstp %st(0)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_sqrt
_syst_floatx80_sqrt:
	fldt 8(%esp)
	fsqrt
	movl 4(%esp),%eax
	fstpt (%eax)
	ret $4

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_eq
_syst_floatx80_eq:
	fldt 16(%esp)
	fldt 4(%esp)
	fucompp
	fnstsw %ax
	andw $17664,%ax
	cmpw $16384,%ax
	seteb %al
	movzb %al,%eax
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_le
_syst_floatx80_le:
	fldt 4(%esp)
	fldt 16(%esp)
	fcompp
	fnstsw %ax
	notl %eax
	shrl $8,%eax
	andl $1,%eax
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_lt
_syst_floatx80_lt:
	fldt 4(%esp)
	fldt 16(%esp)
	fcompp
	fnstsw %ax
	andw $17664,%ax
	setzb %al
	movzb %al,%eax
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_eq_signaling
_syst_floatx80_eq_signaling:
	fldt 16(%esp)
	fldt 4(%esp)
	fcompp
	fnstsw %ax
	andw $17664,%ax
	cmpw $16384,%ax
	seteb %al
	movzb %al,%eax
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_le_quiet
_syst_floatx80_le_quiet:
	fldt 4(%esp)
	fldt 16(%esp)
	fucompp
	fnstsw %ax
	notl %eax
	shrl $8,%eax
	andl $1,%eax
	ret

/*
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
*/
	.align 2
.globl _syst_floatx80_lt_quiet
_syst_floatx80_lt_quiet:
	fldt 4(%esp)
	fldt 16(%esp)
	fucompp
	fnstsw %ax
	andw $17664,%ax
	setzb %al
	movzb %al,%eax
	ret

