#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	systat
CFLAGS+=-I${.CURDIR}/../../sys
SRCS=	cmds.c cmdtab.c devs.c fetch.c iostat.c keyboard.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c icmp.c icmp6.c \
	mode.c ip.c ip6.c tcp.c \
	vmstat.c
CFLAGS+=-DINET6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBM} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lcurses -ltermcap -lm -lkvm -ldevstat
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
