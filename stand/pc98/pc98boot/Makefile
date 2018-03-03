# $FreeBSD$

.include <bsd.init.mk>

FILES=	${BOOT}
CLEANFILES+= ${BOOT} ${BOOT}.part

BOOT=	pc98boot

.if exists(${BOOTOBJ}/pc98/boot0)
BOOT0=	${BOOTOBJ}/pc98/boot0/boot0
.else
BOOT0=	${BOOTSRC}/pc98/boot0/boot0
.endif
.if exists(${BOOTOBJ}/pc98/boot0.5)
BOOT05=	${BOOTOBJ}/pc98/boot0.5/boot0.5
.else
BOOT05=	${BOOTSRC}/pc98/boot0.5/boot0.5
.endif

${BOOT}: ${BOOT0} ${BOOT05} ${BOOT}.part
	cat ${BOOT0} ${BOOT}.part ${BOOT05} > ${.TARGET}

${BOOT}.part:
	${DD} if=/dev/zero of=${.TARGET} bs=512 count=1

.include <bsd.prog.mk>
