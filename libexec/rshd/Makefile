# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $Id: Makefile,v 1.4 1994/11/20 23:20:00 wollman Exp $

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

# For login_cap handling
CFLAGS+=-DLOGIN_CAP
DPADD+=	${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
