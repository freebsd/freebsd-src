# $FreeBSD$

ORG=		0x0

LDR=		liloldr
BSECT=		lilobsect
BOOT=		liloboot
PROG=		${BOOT}
NOMAN=	1
STRIP=
BINDIR?=	/boot

.if exists(${.OBJDIR}/../loader)
LOADER=	${.OBJDIR}/../loader/loader
.else
LOADER=	${.CURDIR}/../loader/loader
.endif

.if exists(${LOADER})
LOADER_SIZE !=	wc -c ${LOADER} | awk '{print int(($$1 + 15) / 16)}'
AFLAGS=		--defsym LOADER_SIZE=${LOADER_SIZE}
.endif

${BOOT}: ${BSECT} ${LDR} ${LOADER}
	cat ${BSECT} ${LDR} ${LOADER} > ${.TARGET}.tmp
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

${BSECT}: ${BSECT}.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o ${BSECT}.out ${BSECT}.o
	dd if=${BSECT}.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o ${BSECT}.out ${BSECT}.o
	objcopy -S -O binary ${BSECT}.out ${.TARGET}
.endif


${BSECT}.o: ${BSECT}.s ${LOADER}

CLEANFILES+= ${LDR} ${LDR}.out ${LDR}.o \
	${BSECT} ${BSECT}.out ${BSECT}.o ${BOOT}.tmp

.include <bsd.prog.mk>
