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

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb -lcrypto -lcom_err
DISTRIBUTION=	krb4
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
