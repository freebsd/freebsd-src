# Common defines for all of /sys/boot/i386/
#
# $FreeBSD$

LOADER_ADDRESS?=	0x200000
CFLAGS+=		-mpreferred-stack-boundary=2
