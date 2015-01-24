# $FreeBSD$

ELFTCDIR=	${.CURDIR}/../../contrib/elftoolchain
READELFDIR=	${ELFTCDIR}/readelf

.PATH: ${READELFDIR}

PROG=	readelf

LIBADD=	dwarf elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
