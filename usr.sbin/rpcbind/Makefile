#	$NetBSD: Makefile,v 1.3 2000/06/20 13:56:43 fvdl Exp $
#	$FreeBSD$

PROG=	rpcbind
CFLAGS+=	-I${LIBCRPCDIR} -I${LIBCINCLUDE} -DPORTMAP -DINET6 -DLIBWRAP
MAN=	rpcbind.8
SRCS=	check_bound.c rpcb_stat.c rpcb_svc_4.c rpcbind.c pmap_svc.c \
	rpcb_svc.c rpcb_svc_com.c security.c warmstart.c util.c \
	rpc_generic.c

LIBCDIR=        ${.CURDIR}/../../lib/libc
LIBCRPCDIR=     ${LIBCDIR}/rpc
LIBCINCLUDE=    ${LIBCDIR}/include


LDADD+= -lwrap -lutil
DPADD+= ${LIBWRAP} ${LIBUTIL}

.PATH:  ${LIBCRPCDIR}

.include <bsd.prog.mk>
