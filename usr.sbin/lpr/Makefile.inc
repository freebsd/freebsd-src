# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
