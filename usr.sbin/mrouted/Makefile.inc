# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.4 1999/08/28 01:17:02 peter Exp $

CFLAGS+=	-DRSRR

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
