.include <bsd.init.mk>

PROG=	${LDR}
INTERNALPROG=
FILES=	${BOOT}
MAN=	${BOOT}.8
SRCS=	${LDR}.S
CLEANFILES+= ${BOOT}

BOOT=	pxeboot
LDR=	pxeldr
ORG=	0x7c00
LOADER=	loader

.if defined(BOOT_PXELDR_PROBE_KEYBOARD)
CFLAGS+=-DPROBE_KEYBOARD
.endif

.if defined(BOOT_PXELDR_ALWAYS_SERIAL)
CFLAGS+=-DALWAYS_SERIAL
.endif

CFLAGS+=-I${BOOTSRC}/i386/common

.if defined(PXEBOOT_DEFAULT_INTERP)
L=${PXEBOOT_DEFAULT_INTERP}
.else
L=${LOADER_DEFAULT_INTERP}
.endif
LOADERBIN= ${BOOTOBJ}/i386/loader_${L}/loader_${L}.bin
# pxeboot runs in an environment where there's 500k or less of space available
# due to space for packet buffers, network drivers, etc. While some environments
# may have a bit more, the limit of 500,000 (488k) provides enough margin to
# work in a huge array of environments. Larger values may work for specific
# environments.
PXEBOOTSIZE?=500000

CLEANFILES+= ${BOOT}.tmp

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	${DD} if=${.TARGET}.tmp of=${.TARGET} bs=2048 conv=sync
	rm ${.TARGET}.tmp

LDFLAGS+=${LDFLAGS_BIN}

CLEANFILES+= ${LOADER}

# Note: pxeldr.S assumes a.out and will require changes for ELF if the output
# format is changed to ELF.
${LOADER}: ${LOADERBIN} ${BTXLDR} ${BTXKERN}
	@set -- `ls -l ${LOADERBIN}` ; x=$$((${PXEBOOTSIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
	    -b ${BTXKERN} ${LOADERBIN}

.include <bsd.prog.mk>
