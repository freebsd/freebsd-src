# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h

opt_compat.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
