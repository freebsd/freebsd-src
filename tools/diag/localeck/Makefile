# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
