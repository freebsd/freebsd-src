# $FreeBSD: src/tools/diag/localeck/Makefile,v 1.3 2002/03/26 13:02:15 ru Exp $

PROG=	localeck
NOMAN=	noman
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
