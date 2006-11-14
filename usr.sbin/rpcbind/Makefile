#	$NetBSD: Makefile,v 1.3 2000/06/20 13:56:43 fvdl Exp $
# $FreeBSD$

LIBCDIR=        ${.CURDIR}/../../lib/libc
LIBCRPCDIR=     ${LIBCDIR}/rpc
LIBCINCLUDE=    ${LIBCDIR}/include

.PATH: ${LIBCRPCDIR}

PROG=	rpcbind
MAN=	rpcbind.8
SRCS=	check_bound.c rpcb_stat.c rpcb_svc_4.c rpcbind.c pmap_svc.c \
	rpcb_svc.c rpcb_svc_com.c security.c warmstart.c util.c \
	rpc_generic.c

CFLAGS+= -I${LIBCRPCDIR} -I${LIBCINCLUDE} -DPORTMAP -DLIBWRAP

.if !defined(NO_INET6)
CFLAGS+= -DINET6
.endif

DPADD=	${LIBWRAP} ${LIBUTIL}
LDADD=	-lwrap -lutil

.include <bsd.prog.mk>
