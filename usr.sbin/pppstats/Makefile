#
#	$Id: Makefile,v 1.1 1993/08/27 02:20:35 rgrimes Exp $
#
PROG=	pppstat
SRCS=	pppstats.c

CFLAGS=	-DNO_DRAND48 -DKVMLIB -DPPP

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

NOMAN=	noman

BINGRP=	kmem

.include <bsd.prog.mk>
