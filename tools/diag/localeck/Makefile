# $FreeBSD$

PROG=	localeck
NOMAN=	noman
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test:	${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
