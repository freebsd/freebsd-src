#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	gettytab.5 ttys.5 getty.8

WARNS?=	1
WFORMAT=0

.include <bsd.prog.mk>

