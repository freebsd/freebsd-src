# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
