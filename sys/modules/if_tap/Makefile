# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	if_tap
SRCS=	if_tap.c opt_inet.h vnode_if.h

CLEANFILES+=	opt_devfs.h opt_inet.h vnode_if.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
