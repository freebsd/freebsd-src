#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/ifmcstat/Makefile,v 1.8 2004/09/13 08:14:55 dd Exp $

PROG=	ifmcstat
MAN=	ifmcstat.8
BINMODE= 550

WARNS?=	2

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
