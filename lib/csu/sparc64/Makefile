# $FreeBSD: src/lib/csu/sparc64/Makefile,v 1.12.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-I${.CURDIR}/../common -I${.CURDIR}/../../libc/include

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
