# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

#CFLAGS+= -DCRYPT

CFLAGS+= -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

# IPv6 support
CFLAGS+= -DINET6

.include <bsd.prog.mk>

.if defined(NOSHARED) && !defined(NOPAM)
DPADD+=	${LIBCRYPTO}
LDADD+=	-L${.OBJDIR}/../../secure/lib/libssh -lssh -lcrypto
.endif
