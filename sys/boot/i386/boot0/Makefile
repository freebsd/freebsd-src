# $FreeBSD$

MAINTAINER=	rnordier

PROG=		boot0
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

M4?=	m4

B0FLAGS=0xf
B0TICKS=0xb6

ORG=	0x600

boot0: boot0.o
.if ${OBJFORMAT} == aout
	${LD} -N -s -T ${ORG} -o boot0.out boot0.o
	dd if=boot0.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o boot0.out boot0.o
	objcopy -S -O binary boot0.out ${.TARGET}
.endif

boot0.o: boot0.s
	${AS} ${AFLAGS} --defsym FLAGS=${B0FLAGS} --defsym TICKS=${B0TICKS} \
		${.IMPSRC} -o ${.TARGET}

CLEANFILES+= boot0.out boot0.o

.include <bsd.prog.mk>
