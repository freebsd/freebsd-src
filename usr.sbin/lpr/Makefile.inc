# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.9 2003/06/11 20:32:52 gad Exp $

CFLAGS+= -DINET6

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
