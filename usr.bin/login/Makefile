# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.25 1998/11/21 18:27:15 jdp Exp $

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
LDADD+=	-lpam
NOSHARED=no
.endif


BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
