# $FreeBSD$

PROG	=	isdnmonitor
SRCS	=	main.c curses.c
MAN	=	isdnmonitor.8

# compile debug support
COPTS	+=	-DDEBUG

DPADD	=	${LIBCURSES}
LDADD	=	-lcurses

.include <bsd.prog.mk>
