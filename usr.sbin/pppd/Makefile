#	From: Id: Makefile.bsd,v 1.4 1994/04/20 00:07:50 paulus Exp
#	$Id$

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c \
	auth.c options.c lock.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

.include <bsd.prog.mk>
