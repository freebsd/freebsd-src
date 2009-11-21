# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
