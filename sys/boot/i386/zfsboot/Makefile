# $FreeBSD$

.PATH:		${.CURDIR}/../boot2 ${.CURDIR}/../common ${.CURDIR}/../../common

FILES=		zfsboot

NM?=		nm

# A value of 0x80 enables LBA support.
BOOT_BOOT1_FLAGS?=	0x80

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x2000

CFLAGS=	-DBOOTPROG=\"zfsboot\" \
	-Os -g \
	-fno-guess-branch-probability \
	-fomit-frame-pointer \
	-fno-unit-at-a-time \
	-mno-align-long-strings \
	-mrtd \
	-DBOOT2 \
	-DFLAGS=${BOOT_BOOT1_FLAGS} \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../common \
	-I${.CURDIR}/../../zfs \
	-I${.CURDIR}/../../../cddl/boot/zfs \
	-I${.CURDIR}/../btx/lib -I. \
	-I${.CURDIR}/../boot2 \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Winline --param max-inline-insns-single=100

LDFLAGS=-static -N --gc-sections

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

CLEANFILES=	zfsboot

zfsboot: zfsboot1 zfsboot2
	cat zfsboot1 zfsboot2 > zfsboot

CLEANFILES+=	zfsboot1 zfsldr.out zfsldr.o

zfsboot1: zfsldr.out
	objcopy -S -O binary zfsldr.out ${.TARGET}

zfsldr.out: zfsldr.o
	${LD} ${LDFLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} zfsldr.o

CLEANFILES+=	zfsboot2 zfsboot.ld zfsboot.ldr zfsboot.bin zfsboot.out \
		zfsboot.o zfsboot.s zfsboot.s.tmp xreadorg.h sio.o

# We currently allow 32768 bytes for zfsboot - in practice it could be
# any size up to 3.5Mb but keeping it fixed size simplifies zfsldr.
# 
BOOT2SIZE=	32768

zfsboot2: zfsboot.ld
	@set -- `ls -l zfsboot.ld`; x=$$((${BOOT2SIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	dd if=zfsboot.ld of=${.TARGET} obs=${BOOT2SIZE} conv=osync

zfsboot.ld: zfsboot.ldr zfsboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l zfsboot.ldr \
	    -o ${.TARGET} -P 1 zfsboot.bin

zfsboot.ldr:
	cp /dev/null ${.TARGET}

zfsboot.bin: zfsboot.out
	objcopy -S -O binary zfsboot.out ${.TARGET}

zfsboot.out: ${BTXCRT} zfsboot.o sio.o drv.o cons.o util.o
	${LD} ${LDFLAGS} -Ttext ${ORG2} -o ${.TARGET} ${.ALLSRC} ${LIBSTAND}

zfsboot.o: zfsboot.s

SRCS=	zfsboot.c xreadorg.h

zfsboot.s: zfsboot.c xreadorg.h ${.CURDIR}/../../zfs/zfsimpl.c
	${CC} ${CFLAGS} -S -o zfsboot.s.tmp ${.CURDIR}/zfsboot.c
	sed -e '/align/d' -e '/nop/d' < zfsboot.s.tmp > zfsboot.s
	rm -f zfsboot.s.tmp

xreadorg.h: zfsldr.out
	${NM} -t d ${.ALLSRC} | awk '/([0-9])+ T xread/ \
	    { x = $$1 - ORG1; \
	    printf("#define XREADORG %#x\n", REL1 + x) }' \
	    ORG1=`printf "%d" ${ORG1}` \
	    REL1=`printf "%d" ${REL1}` > ${.TARGET}

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend zfsboot.s: machine
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.prog.mk>
