#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	gcore
SRCS=	elfcore.c gcore.c

.if ${MACHINE_ARCH} == i386
SRCS+=	aoutcore.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
.endif

.include <bsd.prog.mk>
