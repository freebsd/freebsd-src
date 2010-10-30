# Test generation of unwind tables
	.text
foo:	@ Simple function
	.fnstart
	.save {r4, lr}
	mov r0, #0
	.fnend
foo1:	@ Typical frame pointer prologue
	.fnstart
	.movsp ip
	@mov ip, sp
	.pad #4
	.save {fp, ip, lr}
	@stmfd sp!, {fp, ip, lr, pc}
	.setfp fp, ip, #4
	@sub fp, ip, #4
	mov r0, #1
	.fnend
foo2:	@ Custom personality routine
	.fnstart
	.save {r1, r4, r6, lr}
	@stmfd {r1, r4, r6, lr}
	mov r0, #2
	.personality foo
	.handlerdata
	.word 42
	.fnend
foo3:	@ Saving iwmmxt registers
	.fnstart
	.save {wr12}
	.save {wr13}
	.save {wr11}
	.save {wr10}
	.save {wr10, wr11}
	.save {wr0}
	mov r0, #3
	.fnend
	.code 16
foo4:	@ Thumb frame pointer
	.fnstart
	.save {r7, lr}
	@push {r7, lr}
	.setfp r7, sp
	@mov r7, sp
	.pad #8
	@sub sp, sp, #8
	mov r0, #4
	.fnend
foo5:	@ Save r0-r3 only.
	.fnstart
	.save {r0, r1, r2, r3}
	mov r0, #5
	.fnend
	.code 32
foo6:	@ Nested function with frame pointer
	.fnstart
	.pad #4
	@push {ip}
	.movsp ip, #4
	@mov ip, sp
	.pad #4
	.save {fp, ip, lr}
	@stmfd sp!, {fp, ip, lr, pc}
	.setfp fp, ip, #-8
	@sub fp, ip, #8
	mov r0, #6
	.fnend
