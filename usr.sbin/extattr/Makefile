# $FreeBSD: src/usr.sbin/extattr/Makefile,v 1.1 2002/08/30 08:53:03 phk Exp $

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
