#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.29 2000/02/24 21:06:21 markm Exp $

PROG=	su
SRCS=	su.c

COPTS+=	-DLOGIN_CAP -DSKEY
DPADD=	${LIBUTIL} ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lutil -lskey -lmd -lcrypt

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb -lcrypto -lcom_err
DISTRIBUTION=	krb4
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
