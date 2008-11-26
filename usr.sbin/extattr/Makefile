# $FreeBSD: src/usr.sbin/extattr/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rmextattr
MAN=	rmextattr.8

LINKS+=	${BINDIR}/rmextattr ${BINDIR}/getextattr
LINKS+=	${BINDIR}/rmextattr ${BINDIR}/setextattr
LINKS+=	${BINDIR}/rmextattr ${BINDIR}/lsextattr

MLINKS+=	rmextattr.8 setextattr.8
MLINKS+=	rmextattr.8 getextattr.8
MLINKS+=	rmextattr.8 lsextattr.8

WARNS?=	5

.include <bsd.prog.mk>
