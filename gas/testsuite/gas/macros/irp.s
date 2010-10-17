	.irp	param,1,2,3
	.long	foo\param
	.endr

	.irpc	param,123
	.long	bar\param
	.endr

