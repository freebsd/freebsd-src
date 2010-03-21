# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
