# $NetBSD: Makefile,v 1.1.1.1 2020/06/15 01:52:54 christos Exp $

MKMAN=no

PROGS=srvtest cltest 
SRCS.srvtest = srvtest.c
SRCS.cltest = cltest.c
CPPFLAGS+=-DBLDEBUG
LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
