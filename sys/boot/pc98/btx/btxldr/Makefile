# $FreeBSD: src/sys/boot/pc98/btx/btxldr/Makefile,v 1.2 1999/08/28 00:40:30 peter Exp $

ORG=0x100000
#AFLAGS+=	-x assembler-with-cpp

#AFLAGS+=	--defsym PC98=1 --defsym BTXLDR_VERBOSE=1
AFLAGS+=	--defsym PC98=1

all: btxldr

btxldr: btxldr.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o btxldr.out btxldr.o
	dd if=btxldr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o btxldr.out btxldr.o
	objcopy -S -O binary btxldr.out ${.TARGET}
.endif

btxldr.o: btxldr.s
#	${CC} ${AFLAGS} -c -o ${.TARGET} ${.CURDIR}/btxldr.s
	${AS} ${AFLAGS} -o ${.TARGET} ${.CURDIR}/btxldr.s

CLEANFILES+= btxldr btxldr.out btxldr.o

.include <bsd.prog.mk>
