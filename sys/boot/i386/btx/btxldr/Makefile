# $FreeBSD: src/sys/boot/i386/btx/btxldr/Makefile,v 1.12 2003/06/26 03:51:57 peter Exp $

M4?=	m4
M4FLAGS+=	-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
M4FLAGS+=	-DBTXLDR_VERBOSE
.endif

all: btxldr

btxldr: btxldr.o
	${LD} -N -e start -Ttext ${LOADER_ADDRESS} -o btxldr.out btxldr.o
	objcopy -S -O binary btxldr.out ${.TARGET}

btxldr.o: btxldr.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} btxldr.s ) | \
		${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= btxldr btxldr.out btxldr.o

.include <bsd.prog.mk>
