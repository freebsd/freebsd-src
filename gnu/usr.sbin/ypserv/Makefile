# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	ypserv
SRCS=	dnslookup.c yp_svc.c yp_xdr.c server.c

CFLAGS+=-Wall -DTCP_WRAPPER=0 -DTCPW_FACILITY=LOG_AUTH
CFLAGS+=-DINSTDIR='"/usr/libexec"'

BINOWN=	bin
BINMODE=555

MAN8=

.include <bsd.prog.mk>
