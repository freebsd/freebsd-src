! Zero-sized range descriptors are handled well, but GAS should not emit
! them unnecessarily.  This can happen if .align handling and insn
! assembling does not cater to this specifically and completely.
! Test-case shortened from gcc.c-torture/execute/20000205-1.c.

	.text
_f:
	pt	.L2, tr0
	addi.l	r15, -32, r15
	gettr	tr5, r0
	st.q	r15, 0, r14
	st.q	r15, 24, r0
	st.q	r15, 16, r28
	st.q	r15, 8, r18
	add.l	r15, r63, r14
	add	r2, r63, r1
	beqi	r1, 0, tr0
	pt	_f, tr5
	andi	r1, 128, r28
	.align 2
.L8:
	pt	.L2, tr0
	movi	1, r2
.L2:
	nop

