#	$Id: Makefile,v 1.5 1998/10/13 18:29:18 rnordier Exp $

ORG=0x100000
AFLAGS+=	--assembler-with-cpp

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
	${CC} ${AFLAGS} -c -o ${.TARGET} ${.CURDIR}/btxldr.s

CLEANFILES+= btxldr btxldr.out btxldr.o

.include <bsd.prog.mk>
