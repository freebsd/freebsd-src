# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.2.2.1 2000/07/01 10:11:53 ps Exp $

PROG	=	isdnmonitor
SRCS	=	main.c curses.c
MAN8	=	isdnmonitor.8

# compile debug support
COPTS	+=	-DDEBUG

DPADD	=	${LIBCURSES}
LDADD	=	-lcurses

.include <bsd.prog.mk>
