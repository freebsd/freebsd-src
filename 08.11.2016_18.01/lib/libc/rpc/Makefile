# $NetBSD: Makefile,v 1.2 2013/02/26 17:06:55 christos Exp $

MKMAN=	no

.include <bsd.own.mk>

TESTSDIR=		${TESTSBASE}/lib/libc/rpc

TESTS_C=		t_xdr
SRCS.t_xdr=		${RPCSRCS:.x=_xdr.c} t_xdr.c

TESTS_C+=		t_rpc

RPCSRCS=		h_testbits.x
DPSRCS=			${RPCSRCS:.x=.h}
CLEANFILES+=	${RPCSRCS:.x=.h} ${RPCSRCS:.x=_xdr.c}
CPPFLAGS+=		-I.
RPC_INCS=		${RPCSRCS:.x=.h}
RPC_XDRFILES=	${RPCSRCS:.x=_xdr.c}

.include <bsd.rpc.mk>
.include <bsd.test.mk>

