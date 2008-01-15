# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.10 2005/06/10 06:12:52 des Exp $

CFLAGS+= -DINET6

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
