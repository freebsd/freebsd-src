# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.4.2.4 2002/07/15 14:17:08 des Exp $
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
