#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su
SRCS=	su.c

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.else
COPTS+= -DSKEY
DPADD+= ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD+= -lskey -lmd -lcrypt
.endif

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
