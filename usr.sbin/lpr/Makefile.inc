# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
