# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# This file should be in "kernel_dir/modules/if_tap" directory
# kernel_dir - kernel source root directory (/usr/src/sys)
#
# $FreeBSD: src/sys/modules/if_tap/Makefile,v 1.1.2.1 2000/07/27 13:57:05 nsayer Exp $

.PATH:		${.CURDIR}/../../net
KMOD=		if_tap
SRCS=		if_tap.c opt_devfs.h opt_inet.h vnode_if.h

CLEANFILES+=	opt_devfs.h opt_inet.h vnode_if.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
