#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	su
SRCS=	su.c

CFLAGS+=-DSKEY

LDADD=	-lcrypt -lskey -lmd
DPADD=	${LIBCRYPT} ${LIBSKEY} ${LIBMD}

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
