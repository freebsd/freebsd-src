# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD$

LOADER_ADDRESS?=	0x100000
CFLAGS+=		-ffreestanding -mpreferred-stack-boundary=2
