# Make `routed` for FreeBSD
# $FreeBSD$
# $FreeBSD$

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
SUBDIR= rtquery
LDADD+= -lmd
DPADD+= ${LIBMD}
#COPTS= -g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused

.include <bsd.prog.mk>
