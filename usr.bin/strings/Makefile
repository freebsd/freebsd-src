# $FreeBSD$

.include <src.opts.mk>

ELFTCDIR=	${.CURDIR:H:H}/contrib/elftoolchain

.PATH: ${ELFTCDIR}/strings

PROG=	strings

LIBADD=	elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
