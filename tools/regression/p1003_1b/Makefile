# $FreeBSD: src/tools/regression/p1003_1b/Makefile,v 1.2.2.1 2005/02/13 07:23:34 obrien Exp $

PROG=p1003_1b

SRCS=\
	fifo.c \
	main.c \
	memlock.c \
	prutil.c \
	p26.c \
	sched.c \
	yield.c

NOMAN=

CFLAGS+=-DNO_MEMLOCK
.include <bsd.prog.mk>
