# $FreeBSD$

PROG=	btxldr.out
INTERNALPROG=
NOMAN=
SRCS=	btxldr.s
CLEANFILES= btxldr

M4?=	m4
M4FLAGS+=	-DLOADER_ADDRESS=${LOADER_ADDRESS} -DPC98

.if defined(BTXLDR_VERBOSE)
M4FLAGS+=	-DBTXLDR_VERBOSE
.endif

all: btxldr

LDFLAGS=-N -e start -Ttext ${LOADER_ADDRESS}

btxldr: btxldr.out
	objcopy -S -O binary btxldr.out ${.TARGET}

btxldr.o: btxldr.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} btxldr.s ) | \
		${AS} ${AFLAGS} -o ${.TARGET}

.include <bsd.prog.mk>
