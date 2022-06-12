# $NetBSD: Makefile,v 1.2 2015/01/22 05:03:52 christos Exp $

MKMAN=no

PROGS=srvtest cltest 
SRCS.srvtest = srvtest.c
SRCS.cltest = cltest.c
CPPFLAGS+=-DBLDEBUG
LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
