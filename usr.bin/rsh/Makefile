#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	rsh
SRCS=	rsh.c

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS -DCRYPT
SRCS+=	krcmd.c kcmd.c
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
.PATH:	${.CURDIR}/../rlogin

.include <bsd.prog.mk>
