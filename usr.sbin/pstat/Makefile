#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pstat/Makefile,v 1.11 2002/05/28 06:37:27 des Exp $

PROG=	pstat
WARNS?=	2
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8
WARNS?=	4

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
