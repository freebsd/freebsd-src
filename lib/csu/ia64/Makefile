# $FreeBSD: src/lib/csu/ia64/Makefile,v 1.12.28.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.S crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-Wall -Wno-unused \
		-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o: crt1.S
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
