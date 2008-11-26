# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.12.8.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../netinet

KMOD=	ip_mroute
SRCS=	ip_mroute.c opt_mac.h opt_mrouting.h

CFLAGS+= -DMROUTE_KLD

.if !defined(KERNBUILDDIR)
opt_mrouting.h:
	echo "#define	MROUTING 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
