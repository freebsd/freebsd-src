# $FreeBSD: src/sys/boot/i386/btx/btxldr/Makefile,v 1.18.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
