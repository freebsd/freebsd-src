# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
