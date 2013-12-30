# $FreeBSD$

.PATH:		${.CURDIR}/../boot2 ${.CURDIR}/../common ${.CURDIR}/../../common

FILES=		gptboot
MAN=		gptboot.8

NM?=		nm

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x0

# Decide level of UFS support.
GPTBOOT_UFS?=	UFS1_AND_UFS2
#GPTBOOT_UFS?=	UFS2_ONLY
#GPTBOOT_UFS?=	UFS1_ONLY

CFLAGS=	-DBOOTPROG=\"gptboot\" \
	-O1 \
	-DGPT \
	-D${GPTBOOT_UFS} \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../common \
	-I${.CURDIR}/../btx/lib -I. \
	-I${.CURDIR}/../boot2 \
	-I${.CURDIR}/../../.. \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Winline --param max-inline-insns-single=100

LD_FLAGS=-static -N --gc-sections

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

CLEANFILES=	gptboot

gptboot: gptldr.bin gptboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l gptldr.bin \
	    -o ${.TARGET} gptboot.bin

CLEANFILES+=	gptldr.bin gptldr.out gptldr.o

gptldr.bin: gptldr.out
	objcopy -S -O binary gptldr.out ${.TARGET}

gptldr.out: gptldr.o
	${LD} ${LD_FLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} gptldr.o

CLEANFILES+=	gptboot.bin gptboot.out gptboot.o sio.o gpt.o crc32.o drv.o \
		cons.o util.o

gptboot.bin: gptboot.out
	objcopy -S -O binary gptboot.out ${.TARGET}

gptboot.out: ${BTXCRT} gptboot.o sio.o gpt.o crc32.o drv.o cons.o util.o
	${LD} ${LD_FLAGS} -Ttext ${ORG2} -o ${.TARGET} ${.ALLSRC} ${LIBSTAND}

gptboot.o: ${.CURDIR}/../../common/ufsread.c

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend gptboot.o: machine
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.gptldr.S=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
