#
# $FreeBSD: src/lib/csu/i386-elf/Makefile,v 1.6.2.5 2002/11/23 17:44:29 ru Exp $
#

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-elf -Wall -fkeep-inline-functions \
		-I${.CURDIR}/../common
LDFLAGS+=	-elf

.PATH:		${.CURDIR}/../common

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
