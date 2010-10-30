.syntax unified
.thumb
ldmstm:
	ldmia sp!, {r0}
	ldmia sp!, {r8}
	ldmia r1, {r9}
	ldmia r2!, {ip}
	ldmdb sp!, {r2}
	ldmdb sp!, {r8}
	ldmdb r6, {r4}
	ldmdb r6, {r8}
	ldmdb r2!, {r4}
	ldmdb r2!, {ip}
	stmia sp!, {r3}
	stmia sp!, {r9}
	stmia r3, {ip}
	stmia r4!, {ip}
	stmdb sp!, {r3}
	stmdb sp!, {r9}
	stmdb r7, {r5}
	stmdb r6, {ip}
	stmdb r6!, {fp}
	stmdb r5!, {r8}

