# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD: src/sys/boot/pc98/Makefile.inc,v 1.5.8.2.4.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/boot

LOADER_ADDRESS?=0x100000
CFLAGS+=	-ffreestanding -mpreferred-stack-boundary=2 \
		-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 \
		-Os
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
