# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.11.16.1 2008/10/02 02:57:24 kensmith Exp $
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
PRECIOUSPROG=

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
