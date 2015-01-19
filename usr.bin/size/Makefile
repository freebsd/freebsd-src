# $FreeBSD$

.include <src.opts.mk>

ELFTCDIR=	${.CURDIR}/../../contrib/elftoolchain
SIZEDIR=	${ELFTCDIR}/size

.PATH: ${SIZEDIR}

PROG=	size

LIBADD=	elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
