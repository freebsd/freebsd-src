#       $Id$ 

.PATH:  ${.CURDIR}/../../sys/dev/ccd
KMOD=   ccd_mod
SRCS=   ccd.c ccd.h
NOMAN=
PSEUDO_LKM=
CFLAGS= -I. -I/sys
VFS_LKM=

NCCD?=          4
CLEANFILES+=    ccd.h vnode_if.h

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
