# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
