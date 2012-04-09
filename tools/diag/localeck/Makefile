# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
