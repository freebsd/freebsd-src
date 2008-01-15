# $FreeBSD: src/usr.sbin/mrouted/Makefile.inc,v 1.8 2005/06/10 06:12:53 des Exp $

CFLAGS+= -DRSRR -Dlog=logit

LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a

.include "../Makefile.inc"
