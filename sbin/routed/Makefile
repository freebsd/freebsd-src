# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.5.2.2 2000/02/17 04:26:33 peter Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN8=   routed.8
SUBDIR= rtquery
LDADD+= -lmd
DPADD+= ${LIBMD}
#COPTS= -g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused

.include <bsd.prog.mk>
