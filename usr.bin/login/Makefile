#	@(#)Makefile	5.6 (Berkeley) 6/24/90

PROG=	login
SRCS=	klogin.c login.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINOWN=	root
BINMODE=4555

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif

.include <bsd.prog.mk>
