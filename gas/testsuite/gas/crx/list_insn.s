# Instructions including a register list (opcode is represented as a mask).
 .data
foodata: .word 42
	 .text
footext:

	.global push
push:
push ra, {r3, r4}
push r2

	.global pushx
pushx:
pushx sp, {r0, r1, r2, r3, r4, r5, r6, r7}
pushx r6, {hi, lo}

	.global pop
pop:
pop r0, {r10}
pop r2

	.global popx
popx:
popx sp, {r0, r1, r3, r4, r5, r6, r7}
popx r7, {lo, hi}

	.global popret
popret:
popret r13, {ra, r1}
popret ra

	.global loadm
loadm:
loadm r0, {r1, r6}

	.global loadma
loadma:
loadma r13, {u12, u4, u2}

	.global storm
storm:
storm r15, {ra}

	.global storma
storma:
storma r3, {u0, u2}

