#       $Id: Makefile,v 1.7 1998/10/16 04:30:42 peter Exp $ 

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
	echo "#define DEVFS 1" > opt_devfs.h

.include <bsd.kmod.mk>
