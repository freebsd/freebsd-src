#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	su
SRCS=	su.c

COPTS+=	-DSKEY
.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif

LDADD=	-lskey -lmd -lcrypt
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT}

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
