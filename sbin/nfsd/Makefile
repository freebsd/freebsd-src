#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	nfsd
CFLAGS+=-DNFS
MAN8=	nfsd.8
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
