# $FreeBSD$

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain

PROG=	controlelf
SRCS=	controlelf.c

LIBADD=	elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

WARNS?=	6

.include <bsd.prog.mk>
