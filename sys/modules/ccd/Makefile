#       $Id: Makefile,v 1.6 1998/02/01 18:12:12 bde Exp $ 

.PATH:  ${.CURDIR}/../../dev/ccd
KMOD=   ccd
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=

NCCD?=          4
CLEANFILES+=    ccd.h vnode_if.h vnode_if.c

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
