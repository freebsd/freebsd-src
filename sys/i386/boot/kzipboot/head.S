/*
 * Boot unpacker startup routine.
 * Copyright (C) Serge Vakulenko
 */
	.text
start:
	cli                             # disable interrupts
	call    _boot                   # unpack the kernel image
	ljmp    $CSEG, $KADDR           # jump to unpacked kernel
	. = start + 0x500               # skip over warm boot shit
