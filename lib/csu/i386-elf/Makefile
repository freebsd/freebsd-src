#
# $FreeBSD: src/lib/csu/i386-elf/Makefile,v 1.6.2.1 2000/07/10 09:15:28 obrien Exp $
#

SRCS=		crt1.c crtbegin.c crtend.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-elf -Wall -fkeep-inline-functions
LDFLAGS+=	-elf
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

.PATH:		${.CURDIR}/../common

all: ${OBJS} ${SOBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
