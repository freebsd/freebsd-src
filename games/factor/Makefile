#	@(#)Makefile	5.7 (Berkeley) 4/8/91

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes
MAN6=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
