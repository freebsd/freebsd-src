# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.6 2001/07/20 06:19:48 obrien Exp $

PROG=	isdnmonitor
SRCS=	main.c curses.c
MAN=	isdnmonitor.8

# compile debug support
COPTS+=	-DDEBUG

DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
