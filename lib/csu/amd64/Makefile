# $FreeBSD: src/lib/csu/amd64/Makefile,v 1.18.20.2.2.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		Scrt1.o gcrt1.o
CFLAGS+=	-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include
CFLAGS+=	-fno-omit-frame-pointer

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

Scrt1.o: crt1.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o Scrt1.o ${.CURDIR}/crt1.c

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
