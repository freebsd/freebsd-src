# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.5 2004/12/21 08:47:20 ru Exp $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
