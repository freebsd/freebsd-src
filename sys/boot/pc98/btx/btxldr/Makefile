# $FreeBSD: src/sys/boot/pc98/btx/btxldr/Makefile,v 1.8 2004/04/29 08:24:57 nyan Exp $

PROG=	btxldr
INTERNALPROG=
NOMAN=
SRCS=	btxldr.S

CFLAGS+=-DPC98
CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

LDFLAGS=-N -e start -Ttext ${LOADER_ADDRESS} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
