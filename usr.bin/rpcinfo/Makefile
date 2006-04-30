#	from: @(#)Makefile	5.2 (Berkeley) 5/11/90
# $FreeBSD$

PROG=	rpcinfo
SRCS=	rpcinfo.c rpc_generic.c
MAN=	rpcinfo.8

LIBCDIR=	${.CURDIR}/../../lib/libc
LIBCRPCDIR=	${LIBCDIR}/rpc
LIBCINCLUDE=	${LIBCDIR}/include

CFLAGS+=	-I${LIBCRPCDIR} -I${LIBCINCLUDE} -DPORTMAP -DINET6

.PATH:	${LIBCRPCDIR}

.include <bsd.prog.mk>
