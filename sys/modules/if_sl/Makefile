# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.20.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
