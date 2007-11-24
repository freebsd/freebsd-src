# $FreeBSD$

CFLAGS+= -DINET6

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
