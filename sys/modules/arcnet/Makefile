# $FreeBSD$

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
opt_inet.h opt_inet6.h:
	echo "#define INET 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
