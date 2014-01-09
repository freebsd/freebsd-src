# $FreeBSD$

.PATH:		${.CURDIR}/../boot2 ${.CURDIR}/../gptboot \
		${.CURDIR}/../zfsboot ${.CURDIR}/../common \
		${.CURDIR}/../../common

FILES=		gptzfsboot

NM?=		nm

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x0

CFLAGS=	-DBOOTPROG=\"gptzfsboot\" \
	-O1 \
	-DGPT -DBOOT2 \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../common \
	-I${.CURDIR}/../../zfs \
	-I${.CURDIR}/../../../cddl/boot/zfs \
	-I${.CURDIR}/../btx/lib -I. \
	-I${.CURDIR}/../boot2 \
	-I${.CURDIR}/../../.. \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Winline

CFLAGS.gcc+=	--param max-inline-insns-single=100

LD_FLAGS=-static -N --gc-sections

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

CLEANFILES=	gptzfsboot

gptzfsboot: gptldr.bin gptzfsboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l gptldr.bin \
	    -o ${.TARGET} gptzfsboot.bin

CLEANFILES+=	gptldr.bin gptldr.out gptldr.o

gptldr.bin: gptldr.out
	objcopy -S -O binary gptldr.out ${.TARGET}

gptldr.out: gptldr.o
	${LD} ${LD_FLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} gptldr.o

CLEANFILES+=	gptzfsboot.bin gptzfsboot.out zfsboot.o sio.o cons.o \
		drv.o gpt.o util.o

gptzfsboot.bin: gptzfsboot.out
	objcopy -S -O binary gptzfsboot.out ${.TARGET}

gptzfsboot.out: ${BTXCRT} zfsboot.o sio.o gpt.o drv.o cons.o util.o
	${LD} ${LD_FLAGS} -Ttext ${ORG2} -o ${.TARGET} ${.ALLSRC} ${LIBSTAND}

zfsboot.o: ${.CURDIR}/../../zfs/zfsimpl.c

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend zfsboot.o: machine
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.gptldr.S=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
