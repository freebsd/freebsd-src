# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
