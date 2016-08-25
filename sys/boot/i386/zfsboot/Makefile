# $FreeBSD$

.PATH:		${.CURDIR}/../boot2 ${.CURDIR}/../common \
		${.CURDIR}/../../common ${.CURDIR}/../../../crypto/skein

FILES=		zfsboot
MAN=		zfsboot.8

NM?=		nm

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x2000

CFLAGS=	-DBOOTPROG=\"zfsboot\" \
	-O1 \
	-DBOOT2 \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../common \
	-I${.CURDIR}/../../zfs \
	-I${.CURDIR}/../../../cddl/boot/zfs \
	-I${.CURDIR}/../../../crypto/skein \
	-I${.CURDIR}/../btx/lib -I. \
	-I${.CURDIR}/../boot2 \
	-Wall -Waggregate-return -Wbad-function-cast -Wno-cast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Winline

CFLAGS.gcc+=	--param max-inline-insns-single=100

LD_FLAGS=-static -N --gc-sections

LIBSTAND=	${.OBJDIR}/../../libstand32/libstand.a

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

CLEANFILES=	zfsboot

zfsboot: zfsboot1 zfsboot2
	cat zfsboot1 zfsboot2 > zfsboot

CLEANFILES+=	zfsboot1 zfsldr.out zfsldr.o

zfsboot1: zfsldr.out
	${OBJCOPY} -S -O binary zfsldr.out ${.TARGET}

zfsldr.out: zfsldr.o
	${LD} ${LD_FLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} zfsldr.o

CLEANFILES+=	zfsboot2 zfsboot.ld zfsboot.ldr zfsboot.bin zfsboot.out \
		zfsboot.o zfsboot.s zfsboot.s.tmp sio.o cons.o drv.o util.o \
		skein.o skein_block.o

# We currently allow 128k bytes for zfsboot - in practice it could be
# any size up to 3.5Mb but keeping it fixed size simplifies zfsldr.
# 
BOOT2SIZE=	131072

zfsboot2: zfsboot.ld
	@set -- `ls -l zfsboot.ld`; x=$$((${BOOT2SIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	dd if=zfsboot.ld of=${.TARGET} obs=${BOOT2SIZE} conv=osync status=none

zfsboot.ld: zfsboot.ldr zfsboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l zfsboot.ldr \
	    -o ${.TARGET} -P 1 zfsboot.bin

zfsboot.ldr:
	cp /dev/null ${.TARGET}

zfsboot.bin: zfsboot.out
	${OBJCOPY} -S -O binary zfsboot.out ${.TARGET}

zfsboot.out: ${BTXCRT} zfsboot.o sio.o drv.o cons.o util.o skein.o skein_block.o
	${LD} ${LD_FLAGS} -Ttext ${ORG2} -o ${.TARGET} ${.ALLSRC} ${LIBSTAND}

SRCS=	zfsboot.c

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend zfsboot.o: machine
CLEANFILES+=	machine
machine: .NOMETA
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.zfsldr.S=	${CLANG_NO_IAS}
