#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/systat/Makefile,v 1.15.20.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	systat
SRCS=	cmds.c cmdtab.c devs.c fetch.c iostat.c keyboard.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c icmp.c icmp6.c \
	mode.c ip.c ip6.c tcp.c \
	vmstat.c convtbl.c ifcmds.c ifstat.c
CFLAGS+=-DINET6
DPADD=	${LIBCURSES} ${LIBM} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lcurses -lm -ldevstat -lkvm

.include <bsd.prog.mk>
