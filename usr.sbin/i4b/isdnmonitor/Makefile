# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.7 2003/04/04 17:49:14 obrien Exp $

PROG=	isdnmonitor
MAN=	isdnmonitor.8
SRCS=	main.c curses.c

# compile debug support
COPTS+=	-DDEBUG

DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
