# $FreeBSD: src/sys/modules/bridge/Makefile,v 1.3 2003/09/23 17:54:04 sam Exp $

.PATH:	${.CURDIR}/../../net
KMOD=	bridge
SRCS=	bridge.c
SRCS+=	opt_pfil_hooks.h

#
# By default don't enable pfil hooks support.  This means you
# cannot use ipfilter together with the bridge.  To enable it
# uncomment the line below
#
opt_pfil_hooks.h:
#	echo "#define PFIL_HOOKS 1" > opt_pfil_hooks.h
	touch opt_pfil_hooks.h

.include <bsd.kmod.mk>
