#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/nfsd/Makefile,v 1.10.2.2 2001/04/25 10:58:42 ru Exp $

PROG=	nfsd
CFLAGS+=-DNFS
MAN=	nfsd.8

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBCRYPTO}
LDADD+=	-lkrb -lcrypto
DISTRIBUTION=	krb4
.endif

.include <bsd.prog.mk>
