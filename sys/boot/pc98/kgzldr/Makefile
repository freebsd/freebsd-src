# $FreeBSD$

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
