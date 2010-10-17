	.text
	.mode shmedia
start:

	movi	0x1234,r0
	.long	0x12345678
	.word	0x1234, 0
