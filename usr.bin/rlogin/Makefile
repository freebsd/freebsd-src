#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.12 1998/09/19 22:42:05 obrien Exp $

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
