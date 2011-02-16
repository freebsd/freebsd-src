# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.20.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
