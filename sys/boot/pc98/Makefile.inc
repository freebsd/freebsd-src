# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD: src/sys/boot/pc98/Makefile.inc,v 1.1.2.2 2000/12/30 12:01:03 nyan Exp $

LOADER_ADDRESS?=	0x100000
CFLAGS+=		-mpreferred-stack-boundary=2
