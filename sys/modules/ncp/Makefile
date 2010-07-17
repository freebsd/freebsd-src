# $FreeBSD: src/sys/modules/ncp/Makefile,v 1.10.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../netncp

KMOD=	ncp

SRCS=	ncp_conn.c ncp_sock.c ncp_ncp.c ncp_subr.c ncp_crypt.c ncp_mod.c \
	ncp_rq.c ncp_login.c ncp_nls.c opt_ncp.h

.if defined(NCPBURST)
SRCS+=	ncp_burst.c
CFLAGS+= -DNCPBURST
.endif

.include <bsd.kmod.mk>
