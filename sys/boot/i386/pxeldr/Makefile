# $FreeBSD$

MAINTAINER=jhb@FreeBSD.org

ORG=		0x7c00

LDR=		pxeldr
BOOT=		pxeboot
LOADER=		loader
PROG=		${BOOT}
MAN=		${BOOT}.8
STRIP=
BINDIR?=	/boot
M4?=		m4

.if defined(BOOT_PXELDR_PROBE_KEYBOARD)
M4FLAGS+=	-DPROBE_KEYBOARD
.endif

.if exists(${.OBJDIR}/../loader)
LOADERBIN=	${.OBJDIR}/../loader/loader.bin
.else
LOADERBIN=	${.CURDIR}/../loader/loader.bin
.endif

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	dd if=${.TARGET}.tmp of=${.TARGET} obs=2k conv=osync
	rm ${.TARGET}.tmp

${LOADER}: ${LOADERBIN} ${BTXLDR} ${BTXKERN} ${BTXCRT}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
	    -b ${BTXKERN} ${LOADERBIN}

${LDR}: ${LDR}.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o ${LDR}.out ${LDR}.o
	dd if=${LDR}.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o ${LDR}.out ${LDR}.o
	objcopy -S -O binary ${LDR}.out ${.TARGET}
.endif

${LDR}.o: ${LDR}.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} ${LDR}.s) | \
	   ${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= ${LDR} ${LDR}.out ${LDR}.o ${BOOT}.tmp ${LOADER}

.include <bsd.prog.mk>
