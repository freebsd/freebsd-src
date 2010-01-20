# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/md

KMOD=	geom_md
SRCS=	md.c opt_md.h opt_geom.h vnode_if.h
# create a link g_md.ko for backward compatibility
LINKS=	${KMODDIR}/geom_md.ko ${KMODDIR}/g_md.ko

.include <bsd.kmod.mk>
