# $FreeBSD$

PROG=	btxldr.out
INTERNALPROG=
NOMAN=
SRCS=	btxldr.S
CLEANFILES= btxldr

CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

all: btxldr

LDFLAGS=-N -e start -Ttext ${LOADER_ADDRESS}

btxldr: btxldr.out
	objcopy -S -O binary btxldr.out ${.TARGET}

.include <bsd.prog.mk>
