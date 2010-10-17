 .text
	.global foo
foo:
	.long 0x12345678

 .data
	.global bar
bar:
	.long 0x87654321

	.lcomm dummy, 0x12
