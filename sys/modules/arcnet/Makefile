# $FreeBSD: src/sys/modules/arcnet/Makefile,v 1.2 2002/01/11 01:15:58 msmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	arcnet
SRCS=	if_arcsubr.c
SRCS+=	opt_inet.h opt_inet6.h

EXPORT_SYMS=	arc_frag_init	\
		arc_frag_next	\
		arc_ifattach	\
		arc_ifdetach	\
		arc_input	\
		arc_ioctl	\
		arc_isphds	\
		arc_output	\
		arc_storelladdr

opt_inet.h opt_inet6.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
