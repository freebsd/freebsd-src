	;; sequence control operands need to be packed with nop's
	;;  bl, jl, trap, sleep, stop, wait, dbt, bra, jmp, rte, rtd

	.text
	.align 2
test0:
	bl.s test1
	ldi r0, #1
test1:
	jl r1
	ldi r0, #1
test2:
	trap #1
	ldi r0, #1
test3:
	sleep
	ldi r0, #1
test4:
	wait
	ldi r0, #1
test5:
	stop
	ldi r0, #1
test6:
	dbt
	ldi r0, #1
test7:
	bra.s test5
	ldi r0, #1
test8:
	jmp r1
	ldi r0, #1
test9:
	rte
	ldi r0, #1
test10:
	rtd
	ldi r0, #1
