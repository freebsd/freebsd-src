# $FreeBSD$ 

.PATH:  ${.CURDIR}/../../dev/ccd
KMOD=   ccd
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=

NCCD?=          4
CLEANFILES=	ccd.h

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include "${.CURDIR}/../../conf/kmod.mk"
