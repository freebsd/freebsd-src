# $FreeBSD: src/sys/modules/sppp/Makefile,v 1.5 2002/06/17 05:40:49 jhay Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	sppp
SRCS=	if_spppsubr.c slcompress.c
SRCS+=	opt_inet.h opt_inet6.h opt_ipx.h

EXPORT_SYMS=	sppp_attach	\
		sppp_detach	\
		sppp_input	\
		sppp_ioctl	\
		sppp_dequeue	\
		sppp_pick	\
		sppp_isempty	\
		sppp_flush

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

.include <bsd.kmod.mk>
