# $FreeBSD$

MAINTAINER=	rnordier

M4?=	m4

.if defined(PAGING)
M4FLAGS+=	-DPAGING
.endif

.if defined(BOOT_BTX_NOHANG)
BOOT_BTX_FLAGS=0x1
.else
BOOT_BTX_FLAGS=0x0
.endif

AFLAGS+=	--defsym BTX_FLAGS=${BOOT_BTX_FLAGS}

ORG=	0x9000

all: btx

btx: btx.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o btx.out btx.o
	dd if=btx.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o btx.out btx.o
	objcopy -S -O binary btx.out ${.TARGET}
.endif

btx.o: btx.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} btx.s) | \
	   ${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= btx btx.out btx.o

.include <bsd.prog.mk>
