#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/rlogin/Makefile,v 1.13.2.1 1999/08/29 15:32:12 peter Exp $

PROG=	rlogin
SRCS=	rlogin.c
CFLAGS+=-Wall

BINMODE=4555
INSTALLFLAGS=-fschg

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
SRCS+=	krcmd.c kcmd.c rcmd_util.c encrypt.c
DPADD=	${LIBUTIL} ${LIBKRB} ${LIBDES} ${LIBCRYPT}
CFLAGS+=-DKERBEROS -DCRYPT -DHAVE_CONFIG_H \
	-I${.CURDIR}/../../kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/lib/roken \
	-I${.CURDIR}/../../crypto/kerberosIV/appl/bsd
LDADD=	-lutil -lkrb -ldes -lcrypt
DISTRIBUTION=	krb
.PATH:	${.CURDIR}/../../crypto/kerberosIV/appl/bsd
.endif

.include <bsd.prog.mk>
