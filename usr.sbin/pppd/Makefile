#	$Id: Makefile.bsd,v 1.13 1996/10/08 04:33:33 paulus Exp $

BINDIR?= /usr/sbin
# -D_BITYPES is for FreeBSD, which doesn't define anything to
# tell us that u_int32_t gets defined if <sys/types.h> is included.
# Remove for older *BSD systems for which this isn't true.
CFLAGS+= -g -I.. -DHAVE_PATHS_H -D_BITYPES

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
MAN=	pppd.cat8
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}

.include <bsd.prog.mk>
