# $FreeBSD$

PROG=	isdnmonitor
MAN=	isdnmonitor.8
SRCS=	main.c curses.c

# compile debug support
COPTS+=	-DDEBUG

# avoid wacky merging of string constants from
# source code with compile-time timestamp
COPTS+= -fno-merge-constants

DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
