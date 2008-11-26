# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# $FreeBSD: src/sys/modules/if_tap/Makefile,v 1.4.2.2.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_tap
SRCS=  if_tap.c opt_compat.h opt_inet.h vnode_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
