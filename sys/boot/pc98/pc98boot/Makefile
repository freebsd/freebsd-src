# $FreeBSD$

FILES=	${BOOT}
SRCS=	${BOOT0} ${BOOT05}
CLEANFILES= ${BOOT} ${BOOT}.part

BOOT=	pc98boot
BOOT0=	${.CURDIR}/../boot0/boot0
BOOT05=	${.CURDIR}/../boot0.5/boot0.5

${BOOT}: ${SRCS} ${BOOT}.part
	cat ${BOOT0} ${BOOT}.part ${BOOT05} > ${.TARGET}

${BOOT}.part:
	dd if=/dev/zero of=${.TARGET} bs=512 count=1

.include <bsd.prog.mk>
