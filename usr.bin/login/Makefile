# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.8 1994/09/30 13:26:15 csgr Exp $

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_fbtab.c

CFLAGS+=-DLOGIN_ACCESS -DSKEY -DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT} ${LIBSKEY} ${LIBMD}
LDADD=	-lutil -lcrypt -lskey -lmd

.if exists(${DESTDIR}/usr/lib/libkrb.a) && \
	(defined(MAKE_KERBEROS) || defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
SRCS+=	klogin.c
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
