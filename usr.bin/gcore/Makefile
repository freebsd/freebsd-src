#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	gcore
SRCS=	elfcore.c gcore.c
CFLAGS+=-Wall
DPADD=	${LIBKVM}
LDADD=	-lkvm

.if	${MACHINE} != "sparc"
SRCS+=	md-nop.c
.else
SRCS+=	md-${MACHINDE}.c
.endif

.include <bsd.prog.mk>
