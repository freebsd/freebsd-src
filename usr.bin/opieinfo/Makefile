# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.4.2.1 2001/04/25 11:29:30 ru Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opieinfo

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
