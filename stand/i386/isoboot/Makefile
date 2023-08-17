
.include <bsd.init.mk>

.PATH:		${BOOTSRC}/i386/boot2 ${BOOTSRC}/i386/gptboot \
		${BOOTSRC}/i386/common ${SASRC}

FILES=		isoboot
MAN=		isoboot.8

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 115200
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x0

ISOBOOTSIZE?=	30720

CFLAGS+=-DBOOTPROG=\"isoboot\" \
	-O1 \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${LDRSRC} \
	-I${BOOTSRC}/i386/common \
	-I${BOOTSRC}/i386/boot2 \
	-Wall -Waggregate-return -Wbad-function-cast -Wno-cast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Wno-pointer-sign

CFLAGS.gcc+=	--param max-inline-insns-single=100
CFLAGS.clang+=  -Oz ${CLANG_OPT_SMALL}

LD_FLAGS+=${LD_FLAGS_BIN}

CLEANFILES+=	isoboot

isoboot: gptldr.bin isoboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l gptldr.bin \
	    -o ${.TARGET} isoboot.bin
	@set -- `ls -l ${.TARGET}`; x=$$((${ISOBOOTSIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0

CLEANFILES+=	gptldr.bin gptldr.out gptldr.o

gptldr.bin: gptldr.out
	${OBJCOPY} -S -O binary gptldr.out ${.TARGET}

gptldr.out: gptldr.o
	${LD} ${LD_FLAGS} -e start --defsym ORG=${ORG1} -T ${LDSCRIPT} -o ${.TARGET} gptldr.o

CLEANFILES+=	isoboot.bin isoboot.out isoboot.o sio.o drv.o \
		cons.o ${OPENCRYPTO_XTS}

isoboot.bin: isoboot.out
	${OBJCOPY} -S -O binary isoboot.out ${.TARGET}

isoboot.out: ${BTXCRT} isoboot.o sio.o drv.o cons.o ${OPENCRYPTO_XTS}
	${LD} ${LD_FLAGS} --defsym ORG=${ORG2} -T ${LDSCRIPT} -o ${.TARGET} ${.ALLSRC} ${LIBSA32}

.include <bsd.prog.mk>
