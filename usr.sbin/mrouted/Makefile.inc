# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.7 2003/10/30 22:59:41 peter Exp $

CFLAGS+= -DRSRR -Dlog=logit

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
