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
	echo "#define INET6 1" > opt_inet6.h

opt_atalk.h:
	echo "#define NETATALK 1" > opt_atalk.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

.include <bsd.kmod.mk>
