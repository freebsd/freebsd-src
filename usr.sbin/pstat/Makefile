#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pstat/Makefile,v 1.14.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pstat
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8

WARNS?=	3

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
