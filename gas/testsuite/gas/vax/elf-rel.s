	.data

	.byte	data_vax_8
	.word	data_vax_16
	.long	data_vax_32

	.byte	data_vax_8+8
	.word	data_vax_16+16
	.long	data_vax_32+32

	.text
	.globl	x
x:
	.word	0

	calls	$0, b`text_vax_pc8
	calls	$0, w`text_vax_pc16
	calls	$0, l`text_vax_pc32
	calls	$0, text_vax_plt32

	tstl	b`text_vax_pc8
	tstl	w`text_vax_pc16
	tstl	l`text_vax_pc32
	tstl	text_vax_got32

	tstl	b`text_vax_pc8+8
	tstl	w`text_vax_pc16+16
	tstl	l`text_vax_pc32+32
	tstl	text_vax_got32+32
	ret
