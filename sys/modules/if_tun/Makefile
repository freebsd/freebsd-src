# $FreeBSD$

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c opt_devfs.h opt_atalk.h opt_atm.h opt_inet.h opt_inet6.h \
	opt_ipx.h opt_natm.h vnode_if.h
NOMAN=

NBPF?=	1
NTUN?=		2

CFLAGS+= ${PROTOS}

opt_atalk.h:
	echo "#define NETATALK 1" > opt_atalk.h

opt_atm.h:
	echo "#ifdef __i386__" > opt_atm.h
	echo "#define ATM_CORE 1" >> opt_atm.h
	echo "#else" >>opt_atm.h
	echo "#undef ATM_CORE" >>opt_atm.h
	echo "#endif" >>opt_atm.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

opt_natm.h:
	echo "#define NATM 1" > opt_natm.h

.include <bsd.kmod.mk>
