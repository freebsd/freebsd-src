#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/ifmcstat/Makefile,v 1.8.2.1 2007/04/03 00:39:34 bms Exp $

PROG=	ifmcstat
MAN=	ifmcstat.8
BINMODE= 550

WARNS?=	2
CFLAGS+=-DINET6

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
