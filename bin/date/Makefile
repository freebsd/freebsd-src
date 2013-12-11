#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	date
SRCS=	date.c netdate.c vary.c

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
