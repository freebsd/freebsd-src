# $FreeBSD: src/tools/regression/p1003_1b/Makefile,v 1.1 2000/02/16 14:28:39 dufault Exp $

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

COPTS+=-DNO_MEMLOCK
.include <bsd.prog.mk>
