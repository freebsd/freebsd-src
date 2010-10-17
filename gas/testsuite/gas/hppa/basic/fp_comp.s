	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	fcpy,sgl %fr5,%fr10
	fcpy,dbl %fr5,%fr10
	fcpy,quad %fr5,%fr10
	fcpy,sgl %fr20,%fr24
	fcpy,dbl %fr20,%fr24

	fabs,sgl %fr5,%fr10
	fabs,dbl %fr5,%fr10
	fabs,quad %fr5,%fr10
	fabs,sgl %fr20,%fr24
	fabs,dbl %fr20,%fr24

	fsqrt,sgl %fr5,%fr10
	fsqrt,dbl %fr5,%fr10
	fsqrt,quad %fr5,%fr10
	fsqrt,sgl %fr20,%fr24
	fsqrt,dbl %fr20,%fr24

	frnd,sgl %fr5,%fr10
	frnd,dbl %fr5,%fr10
	frnd,quad %fr5,%fr10
	frnd,sgl %fr20,%fr24
	frnd,dbl %fr20,%fr24
	
	fadd,sgl %fr4,%fr8,%fr12
	fadd,dbl %fr4,%fr8,%fr12
	fadd,quad %fr4,%fr8,%fr12
	fadd,sgl %fr20,%fr24,%fr28
	fadd,dbl %fr20,%fr24,%fr28
	fadd,quad %fr20,%fr24,%fr28

	fsub,sgl %fr4,%fr8,%fr12
	fsub,dbl %fr4,%fr8,%fr12
	fsub,quad %fr4,%fr8,%fr12
	fsub,sgl %fr20,%fr24,%fr28
	fsub,dbl %fr20,%fr24,%fr28
	fsub,quad %fr20,%fr24,%fr28

	fmpy,sgl %fr4,%fr8,%fr12
	fmpy,dbl %fr4,%fr8,%fr12
	fmpy,quad %fr4,%fr8,%fr12
	fmpy,sgl %fr20,%fr24,%fr28
	fmpy,dbl %fr20,%fr24,%fr28
	fmpy,quad %fr20,%fr24,%fr28

	fdiv,sgl %fr4,%fr8,%fr12
	fdiv,dbl %fr4,%fr8,%fr12
	fdiv,quad %fr4,%fr8,%fr12
	fdiv,sgl %fr20,%fr24,%fr28
	fdiv,dbl %fr20,%fr24,%fr28
	fdiv,quad %fr20,%fr24,%fr28

	frem,sgl %fr4,%fr8,%fr12
	frem,dbl %fr4,%fr8,%fr12
	frem,quad %fr4,%fr8,%fr12
	frem,sgl %fr20,%fr24,%fr28
	frem,dbl %fr20,%fr24,%fr28
	frem,quad %fr20,%fr24,%fr28

	fmpyadd,sgl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpyadd,dbl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpysub,sgl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpysub,dbl %fr16,%fr17,%fr18,%fr19,%fr20

	xmpyu %fr4,%fr5,%fr6
