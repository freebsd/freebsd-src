# $FreeBSD: src/tools/regression/p1003_1b/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=p1003_1b

SRCS=\
	fifo.c \
	main.c \
	memlock.c \
	prutil.c \
	p26.c \
	sched.c \
	yield.c

NO_MAN=

CFLAGS+=-DNO_MEMLOCK
.include <bsd.prog.mk>
