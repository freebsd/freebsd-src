# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.31 1999/12/18 13:55:14 peter Exp $

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_fbtab.c

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
