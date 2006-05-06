# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# $FreeBSD: src/sys/modules/if_tap/Makefile,v 1.4.2.1 2006/02/10 00:38:02 yar Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_tap
SRCS=	if_tap.c opt_inet.h vnode_if.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
