# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.8 2001/07/20 06:19:57 obrien Exp $

CFLAGS+= -DINET6
CWARNFLAGS=	${BDECFLAGS}

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
