# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $Id: Makefile,v 1.4.6.2 1998/02/18 11:52:17 markm Exp $

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
