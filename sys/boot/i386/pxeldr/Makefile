# $FreeBSD$

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

FILES=	${BOOT}
MAN=	${BOOT}.8
CLEANFILES= ${BOOT}

BOOT=	pxeboot
LDR=	pxeldr
ORG=	0x7c00
LOADER=	loader
M4?=	m4

.if defined(BOOT_PXELDR_PROBE_KEYBOARD)
M4FLAGS+= -DPROBE_KEYBOARD
.endif

.if defined(BOOT_PXELDR_ALWAYS_SERIAL)
M4FLAGS+= -DALWAYS_SERIAL
.endif

.if exists(${.OBJDIR}/../loader)
LOADERBIN= ${.OBJDIR}/../loader/loader.bin
.else
LOADERBIN= ${.CURDIR}/../loader/loader.bin
.endif

CLEANFILES+= ${BOOT}.tmp

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	dd if=${.TARGET}.tmp of=${.TARGET} obs=2k conv=osync
	rm ${.TARGET}.tmp

CLEANFILES+= ${LDR} ${LDR}.out ${LDR}.o

${LDR}: ${LDR}.out
	objcopy -S -O binary ${LDR}.out ${.TARGET}

${LDR}.out: ${LDR}.o
	${LD} -N -e start -Ttext ${ORG} -o ${.TARGET} ${LDR}.o

${LDR}.o: ${LDR}.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} ${LDR}.s) | \
	    ${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= ${LOADER}

${LOADER}: ${LOADERBIN} ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
	    -b ${BTXKERN} ${LOADERBIN}

.include <bsd.prog.mk>
