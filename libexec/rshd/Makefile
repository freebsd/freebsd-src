# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

#CFLAGS+= -DCRYPT

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

# IPv6 support
CFLAGS+= -DINET6

.include <bsd.prog.mk>
