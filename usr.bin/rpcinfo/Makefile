#	from: @(#)Makefile	5.2 (Berkeley) 5/11/90
# $FreeBSD: src/usr.bin/rpcinfo/Makefile,v 1.6.26.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rpcinfo
SRCS=	rpcinfo.c rpc_generic.c
MAN=	rpcinfo.8

LIBCDIR=	${.CURDIR}/../../lib/libc
LIBCRPCDIR=	${LIBCDIR}/rpc
LIBCINCLUDE=	${LIBCDIR}/include

CFLAGS+=	-I${LIBCRPCDIR} -I${LIBCINCLUDE} -DPORTMAP -DINET6

.PATH:	${LIBCRPCDIR}

.include <bsd.prog.mk>
