# Common defines for all of /sys/boot/i386/
#
# $FreeBSD: src/sys/boot/i386/Makefile.inc,v 1.3 2002/05/10 00:53:45 obrien Exp $

LOADER_ADDRESS?=	0x200000
CFLAGS+=		-ffreestanding -mpreferred-stack-boundary=2
