#	$Id: Makefile,v 1.5 1998/10/13 18:29:18 rnordier Exp $

M4?=	m4

ORG=	0x9000

AFLAGS+=	--defsym PC98=1

all: btx

btx: btx.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o btx.out btx.o
	dd if=btx.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o btx.out btx.o
	objcopy -S -O binary btx.out ${.TARGET}
.endif

btx.o: btx.m4 btx.s
	(cd ${.CURDIR}; ${M4} btx.m4 btx.s) | ${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= btx btx.out btx.o

.include <bsd.prog.mk>
