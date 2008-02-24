# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11 2006/07/27 14:52:12 yar Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
