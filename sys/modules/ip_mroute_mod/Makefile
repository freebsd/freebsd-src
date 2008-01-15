# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.12.8.1 2006/02/10 00:38:03 yar Exp $

.PATH: ${.CURDIR}/../../netinet

KMOD=	ip_mroute
SRCS=	ip_mroute.c opt_mac.h opt_mrouting.h

CFLAGS+= -DMROUTE_KLD

.if !defined(KERNBUILDDIR)
opt_mrouting.h:
	echo "#define	MROUTING 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
