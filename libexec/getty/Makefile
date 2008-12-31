#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/getty/Makefile,v 1.11.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	gettytab.5 ttys.5 getty.8
WFORMAT=0

.include <bsd.prog.mk>

