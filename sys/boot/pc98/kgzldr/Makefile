# $FreeBSD: src/sys/boot/pc98/kgzldr/Makefile,v 1.17.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	kgzldr.o
STRIP=
BINMODE=${LIBMODE}
BINDIR=	${LIBDIR}
NO_MAN=

SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
CFLAGS=	-Os
CFLAGS+=-DKZIP
NO_SHARED=
LDFLAGS=-Wl,-r
.PATH:	${.CURDIR}/../../../kern
.PATH:	${.CURDIR}/../../i386/kgzldr

BOOT_COMCONSOLE_PORT?=	0x238
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

.include <bsd.prog.mk>
