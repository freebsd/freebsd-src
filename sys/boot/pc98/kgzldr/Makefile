# $FreeBSD$

SRCS=	boot.c inflate.c lib.c
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
CFLAGS=	-ffreestanding
CFLAGS+=-Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
.PATH:	${.CURDIR}/../../../kern

.PATH:	${.CURDIR}/../../i386/kgzldr
AFLAGS+=--defsym PC98=1

.for asm in start crt sio
OBJS+=	${asm}.o
${asm}.o: ${asm}.s
	as -o ${.TARGET} ${.ALLSRC:M*${asm}*}
.endfor

CLEANFILES=kgzldr.o

BOOT_COMCONSOLE_PORT?=	0x238
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

all: ${OBJS} kgzldr.o

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    kgzldr.o ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
