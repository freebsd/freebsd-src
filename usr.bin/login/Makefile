#	@(#)Makefile	8.1 (Berkeley) 7/19/93

#CFLAGS+=-DKERBEROS
PROG=	login
SRCS=	login.c
#klogin.c
DPADD=	${LIBUTIL}
#${LIBKRB} ${LIBDES}
LDADD=	-lutil -lcrypt
#-lkrb -ldes
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
