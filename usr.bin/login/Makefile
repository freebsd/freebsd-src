# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	login
SRCS=	login.c login_access.c login_fbtab.c
MAN=	login.1 login.access.5

CFLAGS+=-Wall -DLOGIN_ACCESS -DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

.if defined(NOPAM)
CFLAGS+= -DNO_PAM
.else
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

BINMODE=4555
INSTALLFLAGS=-fschg
NEED_LIBNAMES= yes

.include <bsd.prog.mk>
