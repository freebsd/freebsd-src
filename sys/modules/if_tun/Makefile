# $FreeBSD$

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c opt_devfs.h opt_inet.h opt_inet6.h opt_atalk.h opt_ipx.h \
	vnode_if.h
NOMAN=

NBPF?=	1
NTUN?=		2

CFLAGS+= ${PROTOS}

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
	echo "#undef INET6" > opt_inet6.h

opt_atalk.h:
	echo "#undef NETATALK" > opt_atalk.h

opt_ipx.h:
	echo "#undef IPX" > opt_ipx.h

.include <bsd.kmod.mk>
