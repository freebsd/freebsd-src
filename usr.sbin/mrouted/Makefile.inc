# $FreeBSD$

CFLAGS+= -DRSRR -Dlog=logit

LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a

.include "../Makefile.inc"
