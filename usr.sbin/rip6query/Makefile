#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD: src/usr.sbin/rip6query/Makefile,v 1.6.26.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rip6query
MAN=	rip6query.8

WARNS?=	2
CFLAGS+= -DINET6 -I${.CURDIR}/../route6d

.include <bsd.prog.mk>
