	.text
; check that forward references work for all operands.
	inc r0,#fwd1
	set1 0,#fwd1
	bn 0,#fwd1,.
	add r0,#fwd1
	mov r0,(r0,fwd1)
	mov fwd1,#0
	mov rx,#fwd1
	mov 0,#fwd1
	jmpf fwd1
	bge fwd1+.
	bge Rx,#0,fwd1+.
	bge r0,r0,fwd1+.
	br fwd1+.
fwd1	= 1

; check that global references work for those operands that support them
	.globl global
	
	mov global,#0
	mov rx,#global
	mov 0,#global
;	jmpf global
	bge global
	bge Rx,#0,global
	bge r0,r0,global
	br global

; check branch operations to local labels
	bge .L1
	bge Rx,#0,.L1
	bge r0,r0,.L1
	br .L1
.L1:
	bge .L1
	bge Rx,#0,.L1
	bge r0,r0,.L1
	br .L1

; check immediate operands thoroughly
	mov 0,#global+4
	mov 0,#.L1
	mov 0,#.L1+4
	mov 0,#global-.
	mov 0,#global-.L1

	jmpf global
