# $FreeBSD: src/lib/csu/i386-elf/Makefile,v 1.20.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
FILES=		${SRCS:N*.h:R:S/$/.o/g} gcrt1.o
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESMODE=	${LIBMODE}
FILESDIR=	${LIBDIR}
WARNS?=		6
CFLAGS+=	-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include
CLEANFILES=	${FILES}

gcrt1.o: crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

.include <bsd.prog.mk>
