#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/systat/Makefile,v 1.13 2003/01/04 22:07:24 phk Exp $

PROG=	systat
SRCS=	cmds.c cmdtab.c devs.c fetch.c iostat.c keyboard.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c icmp.c mode.c ip.c tcp.c \
	vmstat.c convtbl.c ifcmds.c ifstat.c
DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lcurses -lm -lkvm -ldevstat

.include <bsd.prog.mk>
