# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.8.12.1 2008/10/02 02:57:24 kensmith Exp $

CFLAGS+= -DRSRR -Dlog=logit

LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a

.include "../Makefile.inc"
