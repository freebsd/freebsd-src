#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	gcore
SRCS=	elfcore.c gcore.c
CFLAGS+=-Wall
DPADD=	${LIBKVM}
LDADD=	-lkvm

.if	${MACHINE_ARCH} != "sparc"
SRCS+=	md-nop.c
.else
SRCS+=	md-${MACHINE_ARCH}.c
.endif

.include <bsd.prog.mk>
