# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.12 2001/12/04 02:19:56 obrien Exp $
# $FreeBSD: src/sbin/routed/Makefile,v 1.12 2001/12/04 02:19:56 obrien Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD+= -lmd
DPADD+= ${LIBMD}
#COPTS= -g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused
WARNS=	0

.include <bsd.prog.mk>
