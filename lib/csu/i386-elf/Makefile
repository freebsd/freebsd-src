#
# $FreeBSD: src/lib/csu/i386-elf/Makefile,v 1.6.2.2 2000/10/30 20:32:24 obrien Exp $
#

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-elf -Wall -fkeep-inline-functions \
		-I${.CURDIR}/../common
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
