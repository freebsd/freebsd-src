# $FreeBSD$

PROG=	fdread
SRCS=	fdread.c fdutil.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
