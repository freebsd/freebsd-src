# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.3.2.1 1999/08/29 15:43:49 peter Exp $

CFLAGS+=	-DRSRR

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
