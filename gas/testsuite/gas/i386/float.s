.psize 0
.text
	fadd
	fadd	%st(3)
	fadd	%st(3),%st
	fadd	%st,%st(3)
	fadds	(%ebx)
	faddl	(%ebx)
	fiadds	(%ebx)
	fiaddl	(%ebx)
	faddp
	faddp	%st(3)
	faddp	%st,%st(3)
	fsub
	fsub	%st(3)
	fsub	%st(3),%st
	fsub	%st,%st(3)
	fsubs	(%ebx)
	fsubl	(%ebx)
	fisubs	(%ebx)
	fisubl	(%ebx)
	fsubp
	fsubp	%st(3)
	fsubp	%st,%st(3)
	fsubr
	fsubr	%st(3)
	fsubr	%st(3),%st
	fsubr	%st,%st(3)
	fsubrs	(%ebx)
	fsubrl	(%ebx)
	fisubrs	(%ebx)
	fisubrl	(%ebx)
	fsubrp
	fsubrp	%st(3)
	fsubrp	%st,%st(3)
	fmul
	fmul	%st(3)
	fmul	%st(3),%st
	fmul	%st,%st(3)
	fmuls	(%ebx)
	fmull	(%ebx)
	fimuls	(%ebx)
	fimull	(%ebx)
	fmulp
	fmulp	%st(3)
	fmulp	%st,%st(3)
	fdiv
	fdiv	%st(3)
	fdiv	%st(3),%st
	fdiv	%st,%st(3)
	fdivs	(%ebx)
	fdivl	(%ebx)
	fidivs	(%ebx)
	fidivl	(%ebx)
	fdivp
	fdivp	%st(3)
	fdivp	%st,%st(3)
	fdivr
	fdivr	%st(3)
	fdivr	%st(3),%st
	fdivr	%st,%st(3)
	fdivrs	(%ebx)
	fdivrl	(%ebx)
	fidivrs	(%ebx)
	fidivrl	(%ebx)
	fdivrp
	fdivrp	%st(3)
	fdivrp	%st,%st(3)

	.p2align	4,0
