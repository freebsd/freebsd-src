#       $Id: Makefile,v 1.7 1998/10/16 04:30:45 peter Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c bpfilter.h opt_devfs.h opt_inet.h tun.h vnode_if.h
NOMAN=
CLEANFILES+=	vnode_if.h vnode_if.c

NBPFILTER?=	0
NTUN?=		2

CFLAGS+= ${PROTOS}
CLEANFILES+=	bpfilter.h opt_devfs.h opt_inet.h tun.h

bpfilter.h:
	echo "#define NBPFILTER ${NBPFILTER}" > bpfilter.h

opt_devfs.h:
	touch opt_devfs.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

tun.h:
	echo "#define NTUN ${NTUN}" > tun.h

.include <bsd.kmod.mk>
