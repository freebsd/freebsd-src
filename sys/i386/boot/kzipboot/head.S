/*
 * Leader for kernel
 * This needs to be at the beginning, so the start address is passed
 * to the boot loader
 * Copyright (C) Serge Vakulenko
 */
	.text
	.globl	kzstart
kzstart:
	cli                             # disable interrupts
	call   start			# unpack the kernel image
					# never return here.
	. = kzstart + 0x500		# skip warm boot stuff
