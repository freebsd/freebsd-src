#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	su
SRCS=	su.c

LDADD=	-lmd
DPADD=	${LIBMD} 

#LC_AUTH=-DLOGIN_CAP_AUTH
COPTS+= -DLOGIN_CAP $(LC_AUTH)
LDADD+= -lutil
DPADD+= ${LIBUTIL}

.if !defined(LC_AUTH)
COPTS+=	-DSKEY
LDADD+= -lskey -lcrypt
DPADD+= ${LIBSKEY} ${LIBCRYPT}
.endif

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES)) && !defined(LC_AUTH)
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
