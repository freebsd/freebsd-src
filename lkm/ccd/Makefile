#       $Id: Makefile,v 1.6 1998/02/01 18:12:12 bde Exp $ 

.PATH:  ${.CURDIR}/../../sys/dev/ccd
KMOD=   ccd_mod
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=
PSEUDO_LKM=

NCCD?=          4
CLEANFILES+=    ccd.h vnode_if.h vnode_if.c

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
