#       $Id: Makefile,v 1.5 1998/01/24 02:21:49 eivind Exp $

.PATH:  ${.CURDIR}/../../sys/net
KMOD=   if_tun_mod
SRCS=   if_tun.c bpfilter.h opt_devfs.h opt_inet.h tun.h
NOMAN=
PSEUDO_LKM=

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
