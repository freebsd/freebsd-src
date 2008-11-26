# $FreeBSD: src/tools/test/malloc/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $
PROG=	malloc
SRCS=	main.c
.PATH:	${.CURDIR}/../../../lib/libc/stdlib

NO_MAN=

test:	malloc
	@echo
	@csh -x -c "time ./malloc      500000  2000 8192"
	@csh -x -c "time ./malloc    50000000  2000 8192"
	@csh -x -c "time ./malloc      500000 14000 8192"
	@csh -x -c "time ./malloc    20000000 20000 2048"

.include <bsd.prog.mk>
