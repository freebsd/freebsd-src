/* $FreeBSD: src/sys/boot/sparc64/boot1/_start.s,v 1.2.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
