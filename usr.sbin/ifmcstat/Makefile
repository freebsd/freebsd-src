#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/ifmcstat/Makefile,v 1.1.2.2 2001/07/10 10:00:34 ume Exp $

PROG=	ifmcstat
MAN=	ifmcstat.8

BINMODE=550
LDADD=	-lkvm
DPADD=	${LIBKVM}

CFLAGS+=-DINET6

.include <bsd.prog.mk>
