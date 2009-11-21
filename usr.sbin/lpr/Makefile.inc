# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
