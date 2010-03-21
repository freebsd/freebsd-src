# $FreeBSD: src/lib/csu/arm/Makefile,v 1.1.10.1.2.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		Scrt1.o gcrt1.o
CFLAGS+=	-Wall -Wno-unused \
		-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

Scrt1.o: crt1.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o Scrt1.o ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
