#	$Id: Makefile,v 1.4.2.1 1998/06/23 21:41:21 peter Exp $

CFLAGS+= -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c ipxcp.c upap.c chap.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINOWN=	root

# as per handbook policies section
MAINTAINER=	peter@freebsd.org

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

# Support SPX/IPX - not quite ready
#CFLAGS+=-DIPX_CHANGE
#SRCS+=	ipxcp.c

# Callback Control Protocol
#CFLAGS+=-DCBCP_SUPPORT
#SRCS+=	cbcp.c

# Filter support
#CFLAGS+=-DPPP_FILTER
#LDADD+=	-lpcap
#DPADD+=	${LIBPCAP}

.include <bsd.prog.mk>
