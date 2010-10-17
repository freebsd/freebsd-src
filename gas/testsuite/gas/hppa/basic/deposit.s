	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	zdep %r4,5,10,%r6
	zdep,= %r4,5,10,%r6
	zdep,< %r4,5,10,%r6
	zdep,od %r4,5,10,%r6
	zdep,tr %r4,5,10,%r6
	zdep,<> %r4,5,10,%r6
	zdep,>= %r4,5,10,%r6
	zdep,ev %r4,5,10,%r6

	dep %r4,5,10,%r6
	dep,= %r4,5,10,%r6
	dep,< %r4,5,10,%r6
	dep,od %r4,5,10,%r6
	dep,tr %r4,5,10,%r6
	dep,<> %r4,5,10,%r6
	dep,>= %r4,5,10,%r6
	dep,ev %r4,5,10,%r6

	zvdep %r4,5,%r6
	zvdep,= %r4,5,%r6
	zvdep,< %r4,5,%r6
	zvdep,od %r4,5,%r6
	zvdep,tr %r4,5,%r6
	zvdep,<> %r4,5,%r6
	zvdep,>= %r4,5,%r6
	zvdep,ev %r4,5,%r6

	vdep %r4,5,%r6
	vdep,= %r4,5,%r6
	vdep,< %r4,5,%r6
	vdep,od %r4,5,%r6
	vdep,tr %r4,5,%r6
	vdep,<> %r4,5,%r6
	vdep,>= %r4,5,%r6
	vdep,ev %r4,5,%r6

	vdepi -1,5,%r6
	vdepi,= -1,5,%r6
	vdepi,< -1,5,%r6
	vdepi,od -1,5,%r6
	vdepi,tr -1,5,%r6
	vdepi,<> -1,5,%r6
	vdepi,>= -1,5,%r6
	vdepi,ev -1,5,%r6

	zvdepi -1,5,%r6
	zvdepi,= -1,5,%r6
	zvdepi,< -1,5,%r6
	zvdepi,od -1,5,%r6
	zvdepi,tr -1,5,%r6
	zvdepi,<> -1,5,%r6
	zvdepi,>= -1,5,%r6
	zvdepi,ev -1,5,%r6

	depi -1,4,10,%r6
	depi,= -1,4,10,%r6
	depi,< -1,4,10,%r6
	depi,od -1,4,10,%r6
	depi,tr -1,4,10,%r6
	depi,<> -1,4,10,%r6
	depi,>= -1,4,10,%r6
	depi,ev -1,4,10,%r6

	zdepi -1,4,10,%r6
	zdepi,= -1,4,10,%r6
	zdepi,< -1,4,10,%r6
	zdepi,od -1,4,10,%r6
	zdepi,tr -1,4,10,%r6
	zdepi,<> -1,4,10,%r6
	zdepi,>= -1,4,10,%r6
	zdepi,ev -1,4,10,%r6

