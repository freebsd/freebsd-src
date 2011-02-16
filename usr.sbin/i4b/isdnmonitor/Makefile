# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.9.32.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	isdnmonitor
MAN=	isdnmonitor.8
SRCS=	main.c curses.c

# compile debug support
CFLAGS+= -DDEBUG

# avoid wacky merging of string constants from
# source code with compile-time timestamp
CFLAGS+= -fno-merge-constants

DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
