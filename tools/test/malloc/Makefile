# $FreeBSD: src/tools/test/malloc/Makefile,v 1.3 2002/01/02 06:42:34 kbyanc Exp $
PROG=	malloc
SRCS=	main.c
.PATH:	${.CURDIR}/../../../lib/libc/stdlib

NOMAN=	sorry

test:	malloc
	@echo
	@csh -x -c "time ./malloc      500000  2000 8192"
	@csh -x -c "time ./malloc    50000000  2000 8192"
	@csh -x -c "time ./malloc      500000 14000 8192"
	@csh -x -c "time ./malloc    20000000 20000 2048"

.include <bsd.prog.mk>
