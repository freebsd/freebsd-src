# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.10.12.1 2008/10/02 02:57:24 kensmith Exp $

CFLAGS+= -DINET6

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
