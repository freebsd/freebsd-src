# $FreeBSD$

FILES=	${BOOT}
CLEANFILES= ${BOOT} ${BOOT}.out ${BOOT}.o

BOOT=	mbr
ORG=	0x600

${BOOT}: ${BOOT}.out
	objcopy -S -O binary ${BOOT}.out ${.TARGET}

${BOOT}.out: ${BOOT}.o
	${LD} -N -e start -Ttext ${ORG} -o ${.TARGET} ${BOOT}.o

.include <bsd.prog.mk>
