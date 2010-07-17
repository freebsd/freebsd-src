# $FreeBSD: src/sys/boot/pc98/btx/btxldr/Makefile,v 1.10.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
