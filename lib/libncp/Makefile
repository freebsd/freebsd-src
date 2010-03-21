# $FreeBSD: src/lib/libncp/Makefile,v 1.9.10.1 2010/02/10 00:26:20 kensmith Exp $

LIB=		ncp

SHLIB_MAJOR=	3

DPADD=		${LIBIPX}
LDADD=		-lipx

SRCS=	ncpl_subr.c ncpl_bind.c ncpl_queue.c ncpl_file.c ncpl_misc.c \
	ncpl_net.c ncpl_rcfile.c ncpl_conn.c ncpl_nls.c ncpl_msg.c \
	ncpl_rpc.c ncpl_crypt.c ipx.c sap.c

.include <bsd.lib.mk>
