# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# This file should be in "kernel_dir/modules/if_tap" directory
# kernel_dir - kernel source root directory (/usr/src/sys)
#
# $FreeBSD: src/sys/modules/if_tap/Makefile,v 1.1.2.2 2001/07/24 09:49:42 dd Exp $

.PATH:		${.CURDIR}/../../net
KMOD=		if_tap
SRCS=		if_tap.c opt_inet.h vnode_if.h

CLEANFILES+=	opt_inet.h vnode_if.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
