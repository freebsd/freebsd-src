/* $FreeBSD: src/sys/boot/sparc64/boot1/_start.s,v 1.2.24.1 2008/10/02 02:57:24 kensmith Exp $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
