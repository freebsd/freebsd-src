# $FreeBSD$

LIB=		ncp

SHLIB_MAJOR=	4

DPADD=		${LIBIPX}
LDADD=		-lipx

SRCS=	ncpl_subr.c ncpl_bind.c ncpl_queue.c ncpl_file.c ncpl_misc.c \
	ncpl_net.c ncpl_rcfile.c ncpl_conn.c ncpl_nls.c ncpl_msg.c \
	ncpl_rpc.c ncpl_crypt.c ipx.c sap.c

WARNS?=	0

.include <bsd.lib.mk>
