# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.2 1999/12/14 21:07:41 hm Exp $
PROG	=	isdnmonitor
SRCS	=	main.c curses.c
MAN8	=	isdnmonitor.8

# compile debug support
COPTS	+=	-DDEBUG

LDADD   +=      -lcurses

.include <bsd.prog.mk>
