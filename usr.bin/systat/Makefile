#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/systat/Makefile,v 1.12 2002/02/08 23:07:35 markm Exp $

PROG=	systat
SRCS=	cmds.c cmdtab.c devs.c fetch.c iostat.c keyboard.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c icmp.c mode.c ip.c tcp.c \
	vmstat.c
DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lcurses -lm -lkvm -ldevstat

.include <bsd.prog.mk>
