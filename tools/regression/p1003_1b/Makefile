# $FreeBSD: src/tools/regression/p1003_1b/Makefile,v 1.2 2004/08/13 14:21:49 ru Exp $

PROG=p1003_1b

SRCS=\
	fifo.c \
	main.c \
	memlock.c \
	prutil.c \
	p26.c \
	sched.c \
	yield.c

NOMAN=1

CFLAGS+=-DNO_MEMLOCK
.include <bsd.prog.mk>
