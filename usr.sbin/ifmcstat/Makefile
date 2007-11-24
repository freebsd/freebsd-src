#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	ifmcstat
MAN=	ifmcstat.8
BINMODE= 550

WARNS?=	2
CFLAGS+=-DINET6

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
