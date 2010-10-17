	.text
	.global apuinfo2
apuinfo2:	
	evstdd 29,8(1)
	mfbbear 29
	mfpmr   29, 27
	dcbtstls 1, 29, 28
	rfmci
