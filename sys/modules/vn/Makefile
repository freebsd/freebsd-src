#       $Id: Makefile,v 1.1 1999/01/21 17:18:34 peter Exp $ 

.PATH:  ${.CURDIR}/../../dev/vn
KMOD=   vn
SRCS=   vn.c vn.h vnode_if.h opt_devfs.h
NOMAN=

NVN?=		8
CLEANFILES+=	vn.h vnode_if.h vnode_if.c opt_devfs.h

vn.h:
	echo "#define NVN ${NVN}" > vn.h

opt_devfs.h:
	touch opt_devfs.h

.include <bsd.kmod.mk>
