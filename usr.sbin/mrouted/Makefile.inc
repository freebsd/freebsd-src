# $FreeBSD$

CFLAGS+=	-DRSRR

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
