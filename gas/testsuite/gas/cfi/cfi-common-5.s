	.text
	.cfi_startproc simple

	.subsection 3
	.cfi_startproc simple
	.long 0
	.cfi_def_cfa 0, 16
	.previous

	.long 0
	.cfi_remember_state

	.subsection 3
	.long 0
	.cfi_adjust_cfa_offset -16
	.previous

	.long 0
	.cfi_restore_state
	.cfi_endproc

	.subsection 3
	.cfi_endproc
	.previous
