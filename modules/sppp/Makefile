# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	sppp
SRCS=	if_spppfr.c if_spppsubr.c slcompress.c
SRCS+=	opt_inet.h opt_inet6.h opt_ipx.h

EXPORT_SYMS=	sppp_attach	\
		sppp_detach	\
		sppp_input	\
		sppp_ioctl	\
		sppp_dequeue	\
		sppp_pick	\
		sppp_isempty	\
		sppp_flush

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
