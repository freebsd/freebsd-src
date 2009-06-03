# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../common

# XXX: prevents infinite loop on startup
.if ${MK_CLANG_IS_CC} != "no" && ${CC} == "cc"
CC=		gcc
.endif

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
