# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD: src/sys/boot/pc98/Makefile.inc,v 1.5 2004/02/09 16:01:44 nyan Exp $

BINDIR?=	/boot

LOADER_ADDRESS?=0x100000
CFLAGS+=	-ffreestanding -mpreferred-stack-boundary=2
LDFLAGS+=	-nostdlib

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
