# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id$

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_skey.c login_fbtab.c
DPADD=	${LIBUTIL} ${LIBSKEY}
LDADD=	-lutil -lcrypt -lskey
CFLAGS+=-DLOGIN_ACCESS -DSKEY -DLOGALL

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
SRCS+=	klogin.c
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

