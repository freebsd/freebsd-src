# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.22 1998/11/11 02:16:01 jdp Exp $

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_fbtab.c

CFLAGS+=-Wall -DSKEY -DLOGIN_ACCESS -DLOGALL

.if defined(KLOGIN_PARANOID)
CFLAGS+=-DKLOGIN_PARANOID
.endif

DPADD=	${LIBUTIL} ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lutil -lskey -lmd -lcrypt

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && defined(MAKE_KERBEROS4)
CFLAGS+=-DKERBEROS
SRCS+=	klogin.c
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
