# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.5 2001/07/20 06:20:02 obrien Exp $

CFLAGS+= -DRSRR

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
