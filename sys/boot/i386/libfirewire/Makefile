# $FreeBSD: src/sys/boot/i386/libfirewire/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

LIB=		firewire
INTERNALLIB=

.PATH:		${.CURDIR}/../../../dev/dcons ${.CURDIR}/../../../dev/firewire
SRCS+=		firewire.c fwohci.c dconsole.c
SRCS+=		dcons.c fwcrom.c

CFLAGS+=	-D_BOOT

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../btx/lib
CFLAGS+=	-I${.CURDIR}/../libi386

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_ARCH} == "amd64"
CLEANFILES+=    machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.lib.mk>

.if ${MACHINE_ARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif

