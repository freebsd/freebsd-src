#	@(#)Makefile	8.1 (Berkeley) 7/19/93

CFLAGS+=-DKERBEROS
PROG=	login
SRCS=	klogin.c login.c
DPADD=	${LIBUTIL} ${LIBKRB} ${LIBDES}
LDADD=	-lutil -lkrb -ldes
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
