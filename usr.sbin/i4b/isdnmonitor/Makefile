# $FreeBSD$

PROG	=	isdnmonitor
SRCS	=	main.c curses.c

# compile debug support
COPTS	+=	-DDEBUG

DPADD	=	${LIBCURSES}
LDADD	=	-lcurses

.include <bsd.prog.mk>
