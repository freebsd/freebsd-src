	.irp	param1,1,2
	 .irp	param2,9,8
	  .long	irp_irp_\param1\param2
	 .endr
	.endr

	.irp	param1,1,2
	 .irpc	param2,98
	  .long	irp_irpc_\param1\param2
	 .endr
	.endr

	.irp	param1,1,2
	 .rept	2
	  .long	irp_rept_\param1
	 .endr
	.endr

	.irpc	param1,12
	 .irp	param2,9,8
	  .long	irpc_irp_\param1\param2
	 .endr
	.endr

	.irpc	param1,12
	 .irpc	param2,98
	  .long	irpc_irpc_\param1\param2
	 .endr
	.endr

	.irpc	param1,12
	 .rept	2
	  .long	irpc_rept_\param1
	 .endr
	.endr

	.rept	2
	 .irp	param2,9,8
	  .long	rept_irp_\param2
	 .endr
	.endr

	.rept	2
	 .irpc	param2,98
	  .long	rept_irpc_\param2
	 .endr
	.endr

	.rept	2
	 .rept	2
	  .long	rept_rept
	 .endr
	.endr
