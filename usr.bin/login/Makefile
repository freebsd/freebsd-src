# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.13 1995/05/30 06:31:25 rgrimes Exp $

PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_fbtab.c

#Uncomment to activate login_auth
#Warning: requires src/libexec/login_* auth modules
#LC_AUTH=-DLOGIN_CAP_AUTH
CFLAGS+=-DSKEY -DLOGIN_ACCESS -DLOGALL -DLOGIN_CAP $(LC_AUTH)

.if defined(KLOGIN_PARANOID)
CFLAGS+=-DKLOGIN_PARANOID
.endif

DPADD=	${LIBUTIL} ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lutil -lskey -lmd -lcrypt

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_EBONES) && !defined(LC_AUTH)
CFLAGS+=-DKERBEROS
SRCS+=	klogin.c
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
