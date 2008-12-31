# $FreeBSD: src/sys/boot/i386/kgzldr/Makefile,v 1.22.18.1 2008/11/25 02:59:29 kensmith Exp $

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

BOOT_COMCONSOLE_PORT?=	0x3f8
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

.include <bsd.prog.mk>
