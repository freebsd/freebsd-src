# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../common

# XXX: prevents infinite loop on startup
.if ${MK_CLANG_IS_CC} != "no" && ${CC} == "cc"
CC=		gcc
.endif

SRCS=		crti.S crtn.S
FILES=		${SRCS:N*.h:R:S/$/.o/g} gcrt1.o crt1.o Scrt1.o
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESMODE=	${LIBMODE}
FILESDIR=	${LIBDIR}
CFLAGS+=	-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include
CLEANFILES=	${FILES} crt1_c.o crt1_s.o gcrt1_c.o Scrt1_c.o

gcrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1_c.o ${.CURDIR}/crt1_c.c

gcrt1.o: gcrt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o gcrt1.o -r crt1_s.o gcrt1_c.o

crt1.o:	crt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o crt1.o -r crt1_s.o crt1_c.o
	objcopy --localize-symbol _start1 crt1.o

Scrt1_c.o:	crt1_c.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o Scrt1_c.o ${.CURDIR}/crt1_c.c

Scrt1.o: Scrt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o Scrt1.o -r crt1_s.o Scrt1_c.o
	objcopy --localize-symbol _start1 Scrt1.o

.include <bsd.prog.mk>
