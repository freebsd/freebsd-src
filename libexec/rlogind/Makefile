#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id$

PROG=	rlogind
SRCS=	rlogind.c
MAN8=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
SRCS+=	encrypt.c
CFLAGS+=-DKERBEROS -DCRYPT -DHAVE_CONFIG_H \
	-I${.CURDIR}/../../kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/lib/roken \
	-I${.CURDIR}/../../crypto/kerberosIV/appl/bsd
DPADD=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.PATH:	${.CURDIR}/../../crypto/kerberosIV/appl/bsd
.endif

.include <bsd.prog.mk>
