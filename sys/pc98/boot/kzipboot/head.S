/*
 * First module in a kzipped kernel.
 * This needs to be at the beginning so that the boot loader calls it.
 * It may be overwritten by uncompressing the kernel, so it transfers
 * control to a higher address that won't be overwritten.
 *
 * Copyright (C) Serge Vakulenko
 */
	.text
head:
	jmp	tail
