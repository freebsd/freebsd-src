# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

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

.include <bsd.prog.mk>
