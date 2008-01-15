# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.19.14.1 2006/02/10 00:38:02 yar Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
