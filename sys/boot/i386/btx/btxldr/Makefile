# $FreeBSD: src/sys/boot/i386/btx/btxldr/Makefile,v 1.17 2004/04/27 19:45:16 ru Exp $

PROG=	btxldr
INTERNALPROG=
NOMAN=
SRCS=	btxldr.S

CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

LDFLAGS=-N -e start -Ttext ${LOADER_ADDRESS} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
