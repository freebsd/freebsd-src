#	$Id$

CFLAGS+= -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c ipxcp.c upap.c chap.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

#CFLAGS+=-DIPX_CHANGE
#SRCS+=	ipxcp.c

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

.include <bsd.prog.mk>
