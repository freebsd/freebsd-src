#	@(#)Makefile	8.1 (Berkeley) 7/19/93

#CFLAGS+=-DKERBEROS
PROG=	login
MAN1=	login.1
MAN5=	login.access.5
SRCS=	login.c login_access.c login_skey.c
#klogin.c
DPADD=	${LIBUTIL} ${LIBSKEY}
#${LIBKRB} ${LIBDES}
LDADD=	-lutil -lcrypt -lskey
#-lkrb -ldes
BINOWN=	root
BINMODE=4555
CFLAGS+=	-DLOGIN_ACCESS -DSKEY -DLOGALL
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

