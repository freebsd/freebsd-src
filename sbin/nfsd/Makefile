#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/nfsd/Makefile,v 1.10 2000/02/24 21:01:52 markm Exp $

PROG=	nfsd
CFLAGS+=-DNFS
MAN8=	nfsd.8
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBCRYPTO}
LDADD+=	-lkrb -lcrypto
DISTRIBUTION=	krb4
.endif

.include <bsd.prog.mk>
