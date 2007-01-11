# $FreeBSD: src/sys/modules/md/Makefile,v 1.13.8.1 2006/11/05 08:50:37 hrs Exp $

.PATH: ${.CURDIR}/../../dev/md

KMOD=	geom_md
SRCS=	md.c opt_md.h opt_geom.h vnode_if.h
# create a link g_md.ko for backward compatibility
LINKS=	${KMODDIR}/geom_md.ko ${KMODDIR}/g_md.ko

.include <bsd.kmod.mk>
