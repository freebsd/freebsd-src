# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
