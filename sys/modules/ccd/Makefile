#       $Id: Makefile,v 1.1.1.1 1996/06/12 03:49:15 asami Exp $ 

.PATH:  ${.CURDIR}/../../sys/dev/ccd
KMOD=   ccd_mod
SRCS=   ccd.c ccd.h vnode_if.h
NOMAN=
PSEUDO_LKM=
CFLAGS= -I.

NCCD?=          4
CLEANFILES+=    ccd.h vnode_if.h vnode_if.c

ccd.h:
	echo "#define NCCD ${NCCD}" > ccd.h

.include <bsd.kmod.mk>
