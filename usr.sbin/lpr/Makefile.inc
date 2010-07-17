# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.11.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
