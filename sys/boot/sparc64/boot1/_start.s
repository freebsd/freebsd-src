/* $FreeBSD: src/sys/boot/sparc64/boot1/_start.s,v 1.2.26.1 2008/11/25 02:59:29 kensmith Exp $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
