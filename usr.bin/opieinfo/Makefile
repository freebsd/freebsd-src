# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.10 2002/01/03 21:20:25 ache Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opieinfo

CFLAGS+=-I${.CURDIR}/../../lib/libopie
CFLAGS+=-I${OPIE_DIST}
CFLAGS+=-DINSECURE_OVERRIDE

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
