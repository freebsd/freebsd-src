#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.21 1998/09/19 22:42:05 obrien Exp $

PROG=	su
SRCS=	su.c

COPTS+= -DLOGIN_CAP -DSKEY
LDADD+= -lutil -lskey -md -lcrypt
DPADD+= ${LIBUTIL} ${LIBSKEY} ${LIBMD} ${LIBCRYPT}

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
