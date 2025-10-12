# $NetBSD: Makefile,v 1.1.1.1 2020/06/15 01:52:52 christos Exp $

BINDIR=/sbin

PROGS=blocklistd blocklistctl
MAN.blocklistd=blocklistd.8 blocklistd.conf.5
MAN.blocklistctl=blocklistctl.8
SRCS.blocklistd = blocklistd.c conf.c run.c state.c support.c internal.c
SRCS.blocklistctl = blocklistctl.c conf.c state.c support.c internal.c
DBG=-g

LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
