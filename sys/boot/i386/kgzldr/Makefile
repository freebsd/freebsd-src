# $FreeBSD$

SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
CFLAGS=	-ffreestanding
CFLAGS+=-Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
.PATH:	${.CURDIR}/../../../kern

CLEANFILES=kgzldr.o

BOOT_COMCONSOLE_PORT?=	0x3f8
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

all: ${OBJS} kgzldr.o

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    kgzldr.o ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
