# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkrb -ldes -lcrypt -lcom_err
DISTRIBUTION=	krb
.endif

# For login_cap handling
CFLAGS+=-DLOGIN_CAP -Wall
DPADD+=	${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
