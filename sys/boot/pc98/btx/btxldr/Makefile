# $FreeBSD: src/sys/boot/pc98/btx/btxldr/Makefile,v 1.2.2.1 2000/04/16 03:55:19 nyan Exp $

M4?=	m4
M4FLAGS+=	-DLOADER_ADDRESS=${LOADER_ADDRESS} -DPC98

.if defined(BTXLDR_VERBOSE)
M4FLAGS+=	-DBTXLDR_VERBOSE
.endif

all: btxldr

btxldr: btxldr.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${LOADER_ADDRESS} -o btxldr.out btxldr.o
	dd if=btxldr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${LOADER_ADDRESS} -o btxldr.out btxldr.o
	objcopy -S -O binary btxldr.out ${.TARGET}
.endif

btxldr.o: btxldr.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} btxldr.s ) | \
		${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= btxldr btxldr.out btxldr.o

.include <${.CURDIR}/../../Makefile.inc>
.include <bsd.prog.mk>
