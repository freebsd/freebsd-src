# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $Id: Makefile,v 1.9 1998/08/06 21:36:25 markm Exp $

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES} ${LIBCRYPT}
LDADD=	-lkrb -ldes -lcrypt
DISTRIBUTION=	krb
.endif

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
