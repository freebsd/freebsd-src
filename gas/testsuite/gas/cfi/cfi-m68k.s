#; $ as -o test.o gas-cfi-test.s && gcc -nostdlib -o test test.o

	.text

#; func_locvars
#; - function with a space on the stack 
#;   allocated for local variables

	.type	func_locvars,@function
func_locvars:
	.cfi_startproc
	
	#; alocate space for local vars
	suba.w	#0x1234,%sp
	.cfi_adjust_cfa_offset	0x1234
	
	#; dummy body
	moveq.l	#1,%d0
	
	#; release space of local vars and return
	adda.w	#0x1234,%sp
	.cfi_adjust_cfa_offset	-0x1234
	rts
	.cfi_endproc

#; func_prologue
#; - functions that begins with standard
#;   prologue: "link %a6,#0"

	.type	func_prologue,@function
func_prologue:
	.cfi_startproc
	
	#; prologue, CFI is valid after 
	#; each instruction.
	link	%a6,#0
	.cfi_def_cfa_offset	8
	.cfi_offset		a6,-8
	.cfi_def_cfa_register	a6

	#; function body
	jbsr	func_locvars
	addq.l	#3, %d0

	#; epilogue with valid CFI
	#; (we're better than gcc :-)
	unlk	%a6
	.cfi_def_cfa_register	sp
	rts
	.cfi_endproc

#; main
#; - typical function
	.type	main,@function
main:
	.cfi_startproc
	
	#; only function body that doesn't
	#; touch the stack at all.
	jbsr	func_prologue
	
	#; return
	rts
	.cfi_endproc
