# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD: src/sys/boot/pc98/Makefile.inc,v 1.3 2002/05/14 12:32:59 nyan Exp $

LOADER_ADDRESS?=	0x100000
CFLAGS+=		-ffreestanding -mpreferred-stack-boundary=2
