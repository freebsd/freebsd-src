# $FreeBSD$

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c opt_devfs.h opt_inet.h tun.h vnode_if.h
NOMAN=
CLEANFILES+=	vnode_if.h vnode_if.c

NBPF?=	1
NTUN?=		2

CFLAGS+= ${PROTOS}
CLEANFILES+=	opt_devfs.h opt_inet.h tun.h

opt_devfs.h:
	touch opt_devfs.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

tun.h:
	echo "#define NTUN ${NTUN}" > tun.h

.include <bsd.kmod.mk>
