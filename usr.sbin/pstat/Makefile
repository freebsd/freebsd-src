#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pstat/Makefile,v 1.15.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pstat
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8

WARNS?=	6

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
