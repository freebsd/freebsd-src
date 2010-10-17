
	.data
bogo
	.ALIGN	8
	.WORD  bogo+4	; = 0x4
	.STRING	"\x00\x00\x00{\x00\x00\x01\xC8\x00\x00\x03\x15"
	.data
	.EXPORT	bogo
	.END
