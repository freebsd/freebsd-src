#	$Id: Makefile,v 1.8 1997/08/22 15:50:09 peter Exp $

CFLAGS+= -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c ipxcp.c upap.c chap.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

# as per handbook policies section
MAINTAINER=	peter@freebsd.org

# Support SPX/IPX - not quite ready
#CFLAGS+=-DIPX_CHANGE
#SRCS+=	ipxcp.c

# Callback Control Protocol
CFLAGS+=-DCBCP_SUPPORT
SRCS+=	cbcp.c

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

.include <bsd.prog.mk>
