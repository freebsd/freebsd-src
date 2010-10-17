	.code

	.align 4
	.IMPORT foo,data

; Switch in/out of different rounding modes.
; Also make sure we "optimize" away useless rounding mode relocations
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
	addil   L'foo-0x12345,%r27
	ldo	R'foo-0x12345(%r1),%r1
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
