#
# $FreeBSD: src/lib/csu/alpha/Makefile,v 1.11.2.4 2002/08/07 16:31:47 ru Exp $
#

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-Wall -Wno-unused \
		-I${.CURDIR}/../common

.PATH:		${.CURDIR}/../common

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
