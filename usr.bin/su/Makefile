#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su
SRCS=	su.c

COPTS+=	-DLOGIN_CAP -DSKEY
DPADD=	${LIBUTIL} ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lutil -lskey -lmd -lcrypt

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

.if defined(MAKE_KERBEROS4) && !defined(NO_OPENSSL) && !defined(NOCRYPT)
CFLAGS+=-DKERBEROS4
DPADD+=	${LIBKRB} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb -lcrypto -lcom_err
DISTRIBUTION=	krb4
.endif

.if defined(MAKE_KERBEROS5) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DKERBEROS5
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBROKEN}
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcrypt -lcom_err \
	-L${.OBJDIR}/../../../../kerberos5/lib/libroken -lroken
DISTRIBUTION=	krb5
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
