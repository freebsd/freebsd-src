# $FreeBSD$

.PATH:	${.CURDIR}/../../kern
KMOD	= lkm
SRCS	= kern_lkm.c vnode_if.h opt_devfs.h

CLEANFILES	+= opt_devfs.h vnode_if.c vnode_if.h

opt_devfs.h:
	touch opt_devfs.h

.include <bsd.kmod.mk>
