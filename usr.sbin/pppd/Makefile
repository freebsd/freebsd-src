#	$Id: Makefile.bsd,v 1.7 1995/04/27 00:19:50 paulus Exp $

BINDIR?= /usr/sbin
CFLAGS+= -I.. -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c ccp.c \
	auth.c options.c sys-bsd.c
MAN8=	pppd.8
# The next line is for NetBSD-current systems.
MAN=	pppd.cat8
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

.include <bsd.prog.mk>
