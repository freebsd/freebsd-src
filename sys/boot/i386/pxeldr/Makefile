# $FreeBSD$

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

PROG=	${LDR}
INTERNALPROG=
FILES=	${BOOT}
MAN=	${BOOT}.8
SRCS=	${LDR}.S
CLEANFILES= ${BOOT}

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

.if ${CC:T:Mclang} == "clang"
# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS+=${.IMPSRC:T:Mpxeldr.S:C/^.+$/-no-integrated-as/}
.endif

LOADERBIN= ${.OBJDIR}/../loader/loader.bin

CLEANFILES+= ${BOOT}.tmp

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	dd if=${.TARGET}.tmp of=${.TARGET} obs=2k conv=osync
	rm ${.TARGET}.tmp

LDFLAGS+=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

CLEANFILES+= ${LOADER}

${LOADER}: ${LOADERBIN} ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
	    -b ${BTXKERN} ${LOADERBIN}

.include <bsd.prog.mk>
