# $FreeBSD: src/sys/modules/ncp/Makefile,v 1.10.34.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../netncp

KMOD=	ncp

SRCS=	ncp_conn.c ncp_sock.c ncp_ncp.c ncp_subr.c ncp_crypt.c ncp_mod.c \
	ncp_rq.c ncp_login.c ncp_nls.c opt_ncp.h

.if defined(NCPBURST)
SRCS+=	ncp_burst.c
CFLAGS+= -DNCPBURST
.endif

.include <bsd.kmod.mk>
