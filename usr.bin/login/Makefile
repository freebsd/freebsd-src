# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.24 1998/11/21 02:22:14 jdp Exp $

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_fbtab.c

CFLAGS+=-Wall -DLOGIN_ACCESS -DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT} ${LIBPAM}
LDADD=	-lutil -lcrypt -lpam
NOSHARED=no

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
