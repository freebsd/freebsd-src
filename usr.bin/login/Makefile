# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $Id: Makefile,v 1.16 1997/02/22 19:55:57 peter Exp $

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

DPADD=	${LIBUTIL} ${LIBSKEY} ${LIBCRYPT}
LDADD=	-lutil -lskey -lcrypt

# have to provide libmd.a for libskey.so to use.
.if ${BINFORMAT} != elf
DPADD+=	${LIBMD}
LDADD+=	lmd
.endif

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
