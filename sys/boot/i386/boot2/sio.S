/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 * $FreeBSD$
 */

		.set SIO_PRT,SIOPRT		# Base port
		.set SIO_FMT,SIOFMT		# 8N1
		.set SIO_DIV,(115200/SIOSPD)	# 115200 / SPD

		.globl sio_init
		.globl sio_flush
		.globl sio_putc
		.globl sio_getc
		.globl sio_ischar

/* void sio_init(void) */

sio_init:	movw $SIO_PRT+0x3,%dx		# Data format reg
		movb $SIO_FMT|0x80,%al		# Set format
		outb %al,(%dx)			#  and DLAB
		pushl %edx			# Save
		subb $0x3,%dl			# Divisor latch reg
		movw $SIO_DIV,%ax		# Set
		outw %ax,(%dx)			#  BPS
		popl %edx			# Restore
		movb $SIO_FMT,%al		# Clear
		outb %al,(%dx)			#  DLAB
		incl %edx			# Modem control reg
		movb $0x3,%al			# Set RTS,
		outb %al,(%dx)			#  DTR
		incl %edx			# Line status reg

/* void sio_flush(void) */

sio_flush.0:	call sio_getc.1 		# Get character
sio_flush:	call sio_ischar 		# Check for character
		jnz sio_flush.0 		# Till none
		ret				# To caller

/* void sio_putc(int c) */

sio_putc:	movw $SIO_PRT+0x5,%dx		# Line status reg
		xor %ecx,%ecx			# Timeout
		movb $0x40,%ch			#  counter
sio_putc.1:	inb (%dx),%al			# Transmitter
		testb $0x20,%al 		#  buffer empty?
		loopz sio_putc.1		# No
		jz sio_putc.2			# If timeout
		movb 0x4(%esp,1),%al		# Get character
		subb $0x5,%dl			# Transmitter hold reg
		outb %al,(%dx)			# Write character
sio_putc.2:	ret $0x4			# To caller

/* int sio_getc(void) */

sio_getc:	call sio_ischar 		# Character available?
		jz sio_getc			# No
sio_getc.1:	subb $0x5,%dl			# Receiver buffer reg
		inb (%dx),%al			# Read character
		ret				# To caller

/* int sio_ischar(void) */

sio_ischar:	movw $SIO_PRT+0x5,%dx		# Line status register
		xorl %eax,%eax			# Zero
		inb (%dx),%al			# Received data
		andb $0x1,%al			#  ready?
		ret				# To caller
