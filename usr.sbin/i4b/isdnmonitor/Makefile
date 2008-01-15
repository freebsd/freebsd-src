# $FreeBSD: src/usr.sbin/i4b/isdnmonitor/Makefile,v 1.9 2004/08/13 14:21:49 ru Exp $

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
