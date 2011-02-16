# $FreeBSD: src/sys/boot/efi/libefi/Makefile,v 1.17.10.2.4.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	efi
INTERNALLIB=

SRCS=	delay.c efi_console.c efinet.c efipart.c errno.c handles.c \
	libefi.c time.c

CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH:S/amd64/i386/}
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+= -I${.CURDIR}/../../common

.include <bsd.lib.mk>
