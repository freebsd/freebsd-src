#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}

.if defined(MAKE_KERBEROS4) && !defined(NO_OPENSSL) && !defined(NOCRYPT)
SRCS+=	krcmd.c kcmd.c rcmd_util.c
DPADD=	${LIBUTIL} ${LIBKRB} ${LIBCRYPTO}
CFLAGS+=-DKERBEROS -DCRYPT -DHAVE_CONFIG_H \
	-I${.CURDIR}/../../kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/lib/roken \
	-I${.CURDIR}/../../crypto/kerberosIV/appl/bsd \
	-I${.CURDIR}
LDADD=	-lutil -lkrb -lcrypto
DISTRIBUTION=	krb4
.PATH:	${.CURDIR}/../../crypto/kerberosIV/appl/bsd
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
