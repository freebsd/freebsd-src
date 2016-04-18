# $FreeBSD$

.include <src.opts.mk>

ELFTCDIR=	${.CURDIR:H:H}/contrib/elftoolchain
NMDIR=		${ELFTCDIR}/nm

.PATH: ${NMDIR}

PROG=	nm

LIBADD=	dwarf elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
