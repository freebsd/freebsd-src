#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su
SRCS=	su.c

#LC_AUTH=-DLOGIN_CAP_AUTH
COPTS+= -DLOGIN_CAP $(LC_AUTH)
LDADD+= -lutil
DPADD+= ${LIBUTIL}

.if !defined(LC_AUTH)
COPTS+=	-DSKEY
LDADD+= -lskey -lmd -lcrypt
DPADD+= ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
.endif

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4) \
	&& !defined(LC_AUTH)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
