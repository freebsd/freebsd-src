#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	rlogin
SRCS=	rlogin.c
CFLAGS+=-Wall

#BINOWN=	root
#BINMODE=4555
#INSTALLFLAGS=-fschg

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
SRCS+=	krcmd.c kcmd.c rcmd_util.c encrypt.c
DPADD=	${LIBKRB} ${LIBDES}
CFLAGS+=-DKERBEROS -DCRYPT -DHAVE_CONFIG_H \
	-I${.CURDIR}/../../kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/lib/roken \
	-I${.CURDIR}/../../crypto/kerberosIV/appl/bsd
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.PATH:	${.CURDIR}/../../crypto/kerberosIV/appl/bsd
.endif

.include <bsd.prog.mk>
