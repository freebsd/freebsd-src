# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.15.2.1 2000/11/12 07:03:50 imp Exp $

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

# IPv6 support
CFLAGS+= -DINET6

.include <bsd.prog.mk>

.PATH:	${.CURDIR}/../rlogind
