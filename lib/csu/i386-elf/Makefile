# $FreeBSD: src/lib/csu/i386-elf/Makefile,v 1.18 2003/05/04 18:34:00 obrien Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
WARNS?=		6
CFLAGS+=	-elf -Wall \
		-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include
LDFLAGS+=	-elf

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
