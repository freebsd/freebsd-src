# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# $FreeBSD: src/sys/modules/if_tap/Makefile,v 1.3 2001/01/06 13:59:50 obrien Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_tap
SRCS=	if_tap.c opt_inet.h vnode_if.h

CLEANFILES+=	opt_devfs.h opt_inet.h vnode_if.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
