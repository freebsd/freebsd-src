#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/bin/rcp/Makefile,v 1.16.2.2 2002/07/19 07:54:51 jmallett Exp $

PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}
WARNS=	0
WFORMAT=0

.if defined(MAKE_KERBEROS4) && !defined(NO_OPENSSL) && !defined(NOCRYPT)
SRCS+=	krcmd.c kcmd.c rcmd_util.c
DPADD=	${LIBUTIL} ${LIBKRB} ${LIBCRYPTO}
CFLAGS+=-DCRYPT -DHAVE_CONFIG_H \
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
