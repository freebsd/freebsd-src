/*
 * Boot unpacker startup routine.
 * Copyright (C) Serge Vakulenko
 */
	.text
start:
	cli                             # disable interrupts
	pushl	4(%esp)			# pass howto arg
	call    _boot                   # unpack the kernel image
	popl	%eax			# discard howto arg
	ljmp    $CSEG, $KADDR           # jump to unpacked kernel
	. = start + 0x500               # skip over warm boot shit
