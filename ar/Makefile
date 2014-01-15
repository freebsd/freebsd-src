# $Id: Makefile 2741 2012-12-10 18:47:00Z jkoshy $

TOP=	..

PROG=	ar
SRCS=	ar.c read.c util.c write.c
LSRC=	acplex.l
YSRC=	acpyacc.y

WARNS?=	5

DPADD=	${LIBARCHIVE} ${LIBELFTC} ${LIBELF}
LDADD=	-larchive -lelftc -lelf

CFLAGS+=-I. -I${.CURDIR}

LINKS=	${BINDIR}/ar ${BINDIR}/ranlib

EXTRA_TARGETS=	ranlib

CLEANFILES+= 	${EXTRA_TARGETS}

MAN=	ar.1 ranlib.1 ar.5

all:	${EXTRA_TARGETS}

${EXTRA_TARGETS}:	${PROG}
	ln -s ${PROG} ${.TARGET}

.include "${TOP}/mk/elftoolchain.prog.mk"

.if ${OS_HOST} == "OpenBSD"
CFLAGS+=	-I/usr/local/include
LDFLAGS+=	-L/usr/local/lib
.endif
