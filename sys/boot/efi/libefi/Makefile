# $FreeBSD: src/sys/boot/efi/libefi/Makefile,v 1.17.6.1 2008/11/25 02:59:29 kensmith Exp $

LIB=	efi
INTERNALLIB=

SRCS=	delay.c efi_console.c efifs.c efinet.c errno.c handles.c libefi.c \
	time.c

CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH:S/amd64/i386/}
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+= -I${.CURDIR}/../../common

.include <bsd.lib.mk>
