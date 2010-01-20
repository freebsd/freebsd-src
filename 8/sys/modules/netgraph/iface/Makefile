# $FreeBSD$
# $Whistle: Makefile,v 1.2 1999/01/19 19:39:21 archie Exp $

KMOD=	ng_iface
SRCS= 	ng_iface.c opt_atalk.h opt_inet.h opt_inet6.h opt_ipx.h

.if !defined(KERNBUILDDIR)
opt_atalk.h:
	echo "#define NETATALK 1" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
