#; $ as -o test.o gas-cfi-test.s && gcc -nostdlib -o test test.o

	.text
	.align 4
	.level 1.1

.globl func_locvars
	.type	func_locvars, @function
func_locvars:
	.PROC
	.CALLINFO FRAME=0x1234,NO_CALLS,SAVE_SP,ENTRY_GR=3
	.ENTRY
	.cfi_startproc
	copy %r3,%r1
	copy %r30,%r3
	.cfi_def_cfa_register r3
	stwm %r1,0x1234(%r30)
	.cfi_adjust_cfa_offset 0x1234
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	.cfi_def_cfa_register sp
	bv,n %r0(%r2)
	.cfi_endproc
	.EXIT
	.PROCEND

.globl func_prologue
	.type	func_prologue, @function
func_prologue:
	.PROC
	.CALLINFO FRAME=64,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.ENTRY
	.cfi_startproc
#; This is not ABI-compliant but helps the test to run on both
#; 32-bit and 64-bit targets
	stw %r2,-24(%r30)
	copy %r3,%r1
	copy %r30,%r3
	.cfi_def_cfa_register r3
	.cfi_offset r2, -24
	stwm %r1,64(%r30)
	bl func_locvars,%r2
	nop
	ldw -20(%r3),%r2
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	.cfi_def_cfa_register sp
	bv,n %r0(%r2)
	.cfi_endproc
	.EXIT
	.PROCEND

	.align 4
.globl main
	.type	main, @function
main:
	.PROC
	.CALLINFO CALLS
	.ENTRY
	#; tail call - simple function that doesn't touch the stack
	.cfi_startproc
	b func_prologue
	nop
	.cfi_endproc
	.EXIT
	.PROCEND
