# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
