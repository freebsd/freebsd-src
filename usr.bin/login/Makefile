# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	login
SRCS=	login.c login_access.c login_fbtab.c
MAN=	login.1 login.access.5

CFLAGS+=-DLOGIN_ACCESS -DLOGALL
WARNS?=	2
NO_WERROR=

DPADD=	${LIBUTIL} ${LIBCRYPT} ${LIBPAM}
LDADD=	-lutil -lcrypt ${MINUSLPAM}

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
NEED_LIBNAMES= yes

.include <bsd.prog.mk>
