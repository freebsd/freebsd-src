# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.1.2.4 2002/02/07 15:12:52 ambrisko Exp $

PROG=		ancontrol
MAN=		ancontrol.8

CFLAGS+=	-I${.CURDIR}/../../sys -DANCACHE
LDADD+=		-lmd

WARNS?=		2

.include <bsd.prog.mk>
