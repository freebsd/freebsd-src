# $FreeBSD$

CFLAGS+= -DRSRR -Dlog=logit

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
