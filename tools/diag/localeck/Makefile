# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.3.8.1 2005/02/13 07:23:33 obrien Exp $

PROG=	localeck
NOMAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
