# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.15 2000/02/24 21:18:08 markm Exp $

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

.if defined(NOPAM)
CFLAGS+= -DNO_PAM
.else
#CFLAGS+= -DCRYPT -DNO_PAM
SRCS+=	auth_pam.c
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

# IPv6 support
CFLAGS+= -DINET6

.include <bsd.prog.mk>

.PATH:	${.CURDIR}/../rlogind
