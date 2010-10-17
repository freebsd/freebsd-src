# emrelocs2.s: local symbols and data which causes relocations.

		.text
		.p2align 4

		.word	0, 0

		.ent lcl_fun
lcl_fun:	.word	3
		.end lcl_fun


		.sdata
		.p2align 4

		.word	0, 0

lcl_var:	.word	4

		.p2align 4

		.word	ext_fun
		.word	ext_var
		.word	lcl_fun
		.word	lcl_var

		.dword	ext_fun
		.dword	ext_var
		.dword	lcl_fun
		.dword	lcl_var
