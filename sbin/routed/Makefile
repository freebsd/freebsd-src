# Make `routed` for FreeBSD
# $Revision: 2.15 $
#       $Id$

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN8=   routed.8
SUBDIR= rtquery
LDADD+= -lmd
DPADD+= ${LIBMD}
#COPTS= -g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused

.include <bsd.prog.mk>
