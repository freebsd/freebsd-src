# $FreeBSD: src/sys/boot/i386/boot2/Makefile,v 1.16 1999/11/15 04:23:40 obrien Exp $

PROG=		boot2
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444
CLEANFILES+=	boot1 boot1.out boot1.o \
		boot2.ldr boot2.bin boot2.ld boot2.out boot2.o \
		sio.o

M4?=	m4

# A value of 0x80 enables LBA support.
B1FLAGS=	0

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

.if exists(${.OBJDIR}/../btx)
BTX=	${.OBJDIR}/../btx
.else
BTX=	${.CURDIR}/../btx
.endif

ORG1=	0x7c00
ORG2=	0x1000

CFLAGS=	-elf -I${.CURDIR}/../btx/lib -I. \
	-Os -fno-builtin -fforce-addr -fdata-sections \
	-malign-functions=0 -malign-jumps=0 -malign-loops=0 -mrtd \
	-mpreferred-stack-boundary=2 \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings

LDFLAGS=-nostdlib -static -N

all: boot1 boot2

boot1: boot1.out
	objcopy -S -O binary boot1.out ${.TARGET}

boot1.out: boot1.o
	${LD} ${LDFLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} boot1.o

boot1.o: boot1.m4 boot1.s
	(cd ${.CURDIR}; ${M4} -DFLAGS=${B1FLAGS} boot1.m4 boot1.s) | \
		${AS} ${AFLAGS} -o ${.TARGET}

boot2: boot2.ldr boot2.bin
	btxld -v -E ${ORG2} -f bin -b ${BTX}/btx/btx -l boot2.ldr \
		-o boot2.ld -P 1 boot2.bin
	@ls -l boot2.ld | awk '{ x = 7680 - $$5; \
		print x " bytes available"; if (x < 0) exit 1 }'
	dd if=boot2.ld of=${.TARGET} obs=7680 conv=osync 2>/dev/null

boot2.ldr:
	dd if=/dev/zero of=${.TARGET} bs=512 count=1 2>/dev/null

boot2.bin: boot2.out
	objcopy -S -O binary boot2.out ${.TARGET}

boot2.out: boot2.o sio.o
	${LD} ${LDFLAGS} -Ttext ${ORG2} -o ${.TARGET} \
		${BTX}/lib/crt0.o boot2.o sio.o

sio.o: sio.s
	(cd ${.CURDIR}; ${M4} -DSIOPRT=${BOOT_COMCONSOLE_PORT} \
		-DSIOFMT=${B2SIOFMT} \
		-DSIOSPD=${BOOT_COMCONSOLE_SPEED} sio.s) | \
		${AS} ${AFLAGS} -o ${.TARGET}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		boot1 ${DESTDIR}${BINDIR}/boot1
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		boot2 ${DESTDIR}${BINDIR}/boot2

.include <bsd.prog.mk>
