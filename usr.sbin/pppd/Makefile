#	From: Id: Makefile.bsd,v 1.4 1994/04/20 00:07:50 paulus Exp
#	$Id: Makefile,v 1.1.6.1 1996/03/01 19:34:43 phk Exp $

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c \
	auth.c options.c lock.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

.include <bsd.prog.mk>
