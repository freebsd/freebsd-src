# $FreeBSD$

MAINTAINER=	gallatin@FreeBSD.org

.PATH: ${.CURDIR}/../../alpha/osf1

KMOD=	osf1
SRCS=	osf1_ioctl.c osf1_misc.c osf1_signal.c osf1_sysent.c \
	osf1_mount.c imgact_osf1.c osf1_sysvec.c opt_compat.h \
	opt_simos.h opt_nfs.h vnode_if.h

#CFLAGS+=  -g
EXPORT_SYMS=	_osf1

opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}

opt_osf1.h opt_simos.h opt_nfs.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
