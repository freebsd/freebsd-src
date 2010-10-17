	.globl __start
	.globl .__start

__start:	
	.csect   .data[DS]
	.long    .__start

	.csect .text[pr]
.__start:
	nop
	