#       $Id: Makefile,v 1.9 1999/04/28 01:18:08 msmith Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c bpf.h opt_devfs.h opt_inet.h tun.h vnode_if.h
NOMAN=
CLEANFILES+=	vnode_if.h vnode_if.c

NBPF?=	1
NTUN?=		2

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpf.h opt_devfs.h opt_inet.h tun.h

bpf.h:
	echo "#define NBPF ${NBPF}" > bpf.h

opt_devfs.h:
	touch opt_devfs.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

tun.h:
	echo "#define NTUN ${NTUN}" > tun.h

.include <bsd.kmod.mk>
