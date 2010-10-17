	.text
	.align 2
foo:
	ldi r1,0x3000
	ldi r0, #1
	jl r1
L0:
	ldi r0, #1
	trap #0
	ldi r0, #1
	bl L0
	ldi r0, #1
	jmp r1
	ldi r0, #1
	stop
	ldi r0, #1
	sleep
	ldi r0, #1
	wait
L1:
	ldi r0, #1
	dbt
	ldi r0, #1
	bra L1
	ldi r0, #1
	rte
	ldi r0, #1


