# $FreeBSD$

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
