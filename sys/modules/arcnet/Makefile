# $FreeBSD: src/sys/modules/arcnet/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	arcnet
SRCS=	if_arcsubr.c
SRCS+=	opt_inet.h opt_inet6.h opt_ipx.h

EXPORT_SYMS=	arc_frag_init	\
		arc_frag_next	\
		arc_ifattach	\
		arc_ifdetach	\
		arc_input	\
		arc_ioctl	\
		arc_isphds	\
		arc_output	\
		arc_storelladdr

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
