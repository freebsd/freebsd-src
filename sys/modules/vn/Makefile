# $FreeBSD$ 

.PATH:  ${.CURDIR}/../../dev/vn
KMOD=   vn
SRCS=   vn.c vnode_if.h opt_devfs.h
NOMAN=

CLEANFILES+=	vnode_if.h vnode_if.c opt_devfs.h

opt_devfs.h:
	touch opt_devfs.h

.include <bsd.kmod.mk>
