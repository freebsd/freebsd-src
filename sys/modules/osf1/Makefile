# $FreeBSD: src/sys/modules/osf1/Makefile,v 1.10 2002/08/18 05:45:10 mike Exp $

MAINTAINER=	gallatin@FreeBSD.org

.PATH: ${.CURDIR}/../../alpha/osf1

KMOD=	osf1
SRCS=	osf1_ioctl.c osf1_misc.c osf1_signal.c osf1_sysent.c \
	osf1_mount.c imgact_osf1.c osf1_sysvec.c opt_compat.h \
	opt_simos.h opt_ddb.h opt_mac.h opt_nfs.h device_if.h \
	bus_if.h vnode_if.h

#CFLAGS+=  -g

opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}

.include <bsd.kmod.mk>
