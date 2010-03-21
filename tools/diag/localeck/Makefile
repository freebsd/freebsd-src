# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
