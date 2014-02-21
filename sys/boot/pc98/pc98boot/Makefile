# $FreeBSD$

FILES=	${BOOT}
CLEANFILES= ${BOOT} ${BOOT}.part

BOOT=	pc98boot

.if exists(${.OBJDIR}/../boot0)
BOOT0=	${.OBJDIR}/../boot0/boot0
.else
BOOT0=	${.CURDIR}/../boot0/boot0
.endif
.if exists(${.OBJDIR}/../boot0.5)
BOOT05=	${.OBJDIR}/../boot0.5/boot0.5
.else
BOOT05=	${.CURDIR}/../boot0.5/boot0.5
.endif

${BOOT}: ${BOOT0} ${BOOT05} ${BOOT}.part
	cat ${BOOT0} ${BOOT}.part ${BOOT05} > ${.TARGET}

${BOOT}.part:
	dd if=/dev/zero of=${.TARGET} bs=512 count=1

.include <bsd.prog.mk>
