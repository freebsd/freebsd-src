# $FreeBSD$

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crtbegin.c crtend.c
OBJS=		crt1.o crtbegin.o crtend.o
OBJS+=		gcrt1.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-Wall -Wno-unused
CFLAGS+=	-I${.CURDIR}/../common
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

all: ${OBJS} ${SOBJS}

gcrt1.o:  crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.ALLSRC}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
