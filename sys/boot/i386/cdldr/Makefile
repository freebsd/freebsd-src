# $FreeBSD: src/sys/boot/i386/cdldr/Makefile,v 1.1.2.2 2000/07/07 21:12:33 jhb Exp $

MAINTAINER=jhb@FreeBSD.org

ORG=		0x7c00

LDR=		cdldr
BOOT=		cdboot
PROG=		${BOOT}
NOMAN=
STRIP=
BINDIR?=	/boot


.if exists(${.OBJDIR}/../loader)
LOADER=	${.OBJDIR}/../loader/loader
.else
LOADER=	${.CURDIR}/../loader/loader
.endif

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	dd if=${.TARGET}.tmp of=${.TARGET} obs=2k conv=osync
	rm ${.TARGET}.tmp

${LDR}: ${LDR}.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o ${LDR}.out ${LDR}.o
	dd if=${LDR}.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o ${LDR}.out ${LDR}.o
	objcopy -S -O binary ${LDR}.out ${.TARGET}
.endif

CLEANFILES+= ${LDR} ${LDR}.out ${LDR}.o ${BOOT}.tmp

.include <bsd.prog.mk>
