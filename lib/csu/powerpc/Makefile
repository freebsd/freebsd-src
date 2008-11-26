# $FreeBSD: src/lib/csu/powerpc/Makefile,v 1.9.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-Wall -Wno-unused \
		-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
