#	$FreeBSD$

CFLAGS+= -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c ccp.c \
	auth.c options.c sys-bsd.c
MAN8=	pppd.8
# The next line is for NetBSD-current systems.
MAN=	pppd.cat8
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

.include <bsd.prog.mk>
