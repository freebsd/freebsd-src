# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.15.2.3 2001/04/25 10:40:59 ru Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

#CFLAGS+= -DCRYPT

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

# IPv6 support
CFLAGS+= -DINET6

.include <bsd.prog.mk>
