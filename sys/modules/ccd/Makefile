# $FreeBSD: src/sys/modules/ccd/Makefile,v 1.9 1999/11/28 18:52:42 bde Exp $ 

.PATH:  ${.CURDIR}/../../dev/ccd
KMOD=   ccd
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=

NCCD?=          4
CLEANFILES=	ccd.h

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
