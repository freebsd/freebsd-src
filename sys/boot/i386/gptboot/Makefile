# $FreeBSD$

PROG=		boot2
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444
CLEANFILES+=	boot1 boot1.out boot1.o \
		boot2.ldr boot2.bin boot2.ld boot2.out boot2.o boot2.h \
		boot2.s sio.o divdi3.o moddi3.o qdivrem.o

NM?=		nm

# A value of 0x80 enables LBA support.
B1FLAGS=	0x80

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

.if exists(${.OBJDIR}/../btx)
BTX=	${.OBJDIR}/../btx
.else
BTX=	${.CURDIR}/../btx
.endif

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x1000

# Setting this to anything else gives UFS1+2 support and larger
# boot2 binary.
BOOT2_UFS?=	UFS1_ONLY

CFLAGS=	-elf -ffreestanding -Os -fno-builtin \
	-fno-guess-branch-probability \
	-mrtd \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../btx/lib -I. \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings

.if ${BOOT2_UFS} == "UFS1_ONLY"
CFLAGS+= -D${BOOT2_UFS}
.else

.endif

LDFLAGS=-nostdlib -static -N

all: boot1 boot2

boot1: boot1.out
	objcopy -S -O binary boot1.out ${.TARGET}

boot1.out: boot1.o
	${LD} ${LDFLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} boot1.o

boot1.o: boot1.s
	${CPP} ${CFLAGS} ${.IMPSRC} | \
	    ${AS} ${AFLAGS} --defsym FLAGS=${B1FLAGS} -o ${.TARGET}

boot2.o: boot2.c ${.CURDIR}/../../common/ufsread.c
	${CC} ${CFLAGS} -S -o boot2.s.tmp ${.IMPSRC}
	sed -e '/align/d' -e '/nop/d' < boot2.s.tmp > boot2.s
	rm -f boot2.s.tmp
	${AS} ${AFLAGS} -o boot2.o boot2.s

boot2.h: boot1.out
	${NM} -t d ${.ALLSRC} | awk '/([0-9])+ T xread/ \
	 	{ x = $$1 - ORG1; \
		printf("#define XREADORG %#x\n", REL1 + x) }' \
	 	ORG1=`printf "%d" ${ORG1}` \
		REL1=`printf "%d" ${REL1}` > boot2.h

boot2: boot2.ldr boot2.bin ${BTX}/btx/btx
	btxld -v -E ${ORG2} -f bin -b ${BTX}/btx/btx -l boot2.ldr \
		-o boot2.ld -P 1 boot2.bin
.if ${BOOT2_UFS} == "UFS1_ONLY"
	@ls -l boot2.ld | awk '{ x = 7680 - $$5; \
		print x " bytes available"; if (x < 0) exit 1 }'
	dd if=boot2.ld of=${.TARGET} obs=7680 conv=osync 2>/dev/null
.else
	@ls -l boot2.ld | awk '{ x = 9728 - $$5; \
		print x " bytes available"; if (x < 0) exit 1 }'
	dd if=boot2.ld of=${.TARGET} obs=9728 conv=osync 2>/dev/null
.endif

boot2.ldr:
	dd if=/dev/zero of=${.TARGET} bs=512 count=1 2>/dev/null

boot2.bin: boot2.out
	objcopy -S -O binary boot2.out ${.TARGET}

.if ${BOOT2_UFS} == "UFS1_ONLY"
boot2.out: boot2.o sio.o
.else
boot2.out: boot2.o sio.o divdi3.o moddi3.o qdivrem.o
.endif
	${LD} ${LDFLAGS} -Ttext ${ORG2} -o ${.TARGET} \
		${BTX}/lib/crt0.o ${.ALLSRC}

boot2.o: boot2.h

sio.o: sio.s
	${AS} ${AFLAGS} --defsym SIOPRT=${BOOT_COMCONSOLE_PORT} \
		--defsym SIOFMT=${B2SIOFMT} \
		--defsym SIOSPD=${BOOT_COMCONSOLE_SPEED} \
		${.IMPSRC} -o ${.TARGET}

moddi3.o: ${.CURDIR}/../../../libkern/moddi3.c
	${CC} ${CFLAGS} -I${.CURDIR}/../../../ -c ${.IMPSRC}

divdi3.o: ${.CURDIR}/../../../libkern/divdi3.c
	${CC} ${CFLAGS} -I${.CURDIR}/../../../ -c ${.IMPSRC}

qdivrem.o: ${.CURDIR}/../../../libkern/qdivrem.c
	${CC} ${CFLAGS} -I${.CURDIR}/../../../ -c ${.IMPSRC}

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		boot1 ${DESTDIR}${BINDIR}/boot1
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		boot2 ${DESTDIR}${BINDIR}/boot2

.include <bsd.prog.mk>
