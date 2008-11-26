# $FreeBSD: src/sys/boot/i386/btx/btxldr/Makefile,v 1.18.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	btxldr
INTERNALPROG=
NO_MAN=
SRCS=	btxldr.S

CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

LDFLAGS=-N -e start -Ttext ${LOADER_ADDRESS} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
