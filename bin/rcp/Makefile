#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$FreeBSD$

PROG=	rcp
SRCS=	rcp.c util.c


.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS)	\
	|| defined(MAKE_EBONES))
SRCS+=	kcmd.c krcmd.c
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.PATH:	${.CURDIR}/../../usr.bin/rlogin

.include <bsd.prog.mk>
