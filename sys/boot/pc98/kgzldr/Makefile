# $FreeBSD: src/sys/boot/pc98/kgzldr/Makefile,v 1.14 2004/02/13 15:24:25 nyan Exp $

PROG=	kgzldr.o
STRIP=
BINMODE=${LIBMODE}
BINDIR=	${LIBDIR}
NOMAN=

SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
CFLAGS=	-Os
CFLAGS+=-DKZIP
NOSHARED=
LDFLAGS=-Wl,-r
.PATH:	${.CURDIR}/../../../kern
.PATH:	${.CURDIR}/../../i386/kgzldr

BOOT_COMCONSOLE_PORT?=	0x238

AFLAGS+=--defsym PC98=1
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

.include <bsd.prog.mk>
