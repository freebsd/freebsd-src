#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/systat/Makefile,v 1.14 2003/08/01 20:28:20 dwmalone Exp $

PROG=	systat
SRCS=	cmds.c cmdtab.c devs.c fetch.c iostat.c keyboard.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c icmp.c icmp6.c \
	mode.c ip.c ip6.c tcp.c \
	vmstat.c convtbl.c ifcmds.c ifstat.c
CFLAGS+=-DINET6
DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lcurses -lm -lkvm -ldevstat

.include <bsd.prog.mk>
