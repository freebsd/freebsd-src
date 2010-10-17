	.text
	.global apuinfo1
apuinfo1:	
	evstdd 29,8(1)
	isellt 29, 28, 27
	efsabs 29, 28
	.global _start
_start:
	nop
