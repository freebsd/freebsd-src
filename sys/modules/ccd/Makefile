# $FreeBSD$ 

.PATH:  ${.CURDIR}/../../dev/ccd
KMOD=   ccd
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=

NCCD?=          4
CLEANFILES+=    ccd.h vnode_if.h vnode_if.c

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
