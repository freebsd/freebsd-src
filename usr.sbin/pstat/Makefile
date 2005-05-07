#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pstat/Makefile,v 1.12 2003/04/04 17:49:17 obrien Exp $

PROG=	pstat
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8

WARNS?=	2

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
