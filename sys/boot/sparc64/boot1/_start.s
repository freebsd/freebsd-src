/* $FreeBSD: src/sys/boot/sparc64/boot1/_start.s,v 1.2.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
