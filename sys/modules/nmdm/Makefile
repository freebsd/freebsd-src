# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/nmdm

KMOD=	nmdm
SRCS=	nmdm.c
SRCS+=	opt_compat.h vnode_if.h

opt_compat.h:
	echo "#define COMPAT_43 1" >opt_compat.h

.include <bsd.kmod.mk>
