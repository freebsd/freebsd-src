# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/libamu/Makefile,v 1.4 1999/08/28 01:15:17 peter Exp $

.PATH:		${.CURDIR}/../../../contrib/amd/libamu \
		${.CURDIR}/../../../contrib/amd/conf/transp \
		${.CURDIR}/../../../contrib/amd/conf/mtab \
		${.CURDIR}/../../../contrib/amd/conf/umount

LIB=		amu

SRCS=		hasmntopt.c misc_rpc.c mount_fs.c mtab.c nfs_prot_xdr.c \
		util.c wire.c xutil.c

#		These would be links created by the GNU-style configure
SRCS+=		transp_sockets.c mtab_bsd.c umount_bsd44.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/libamu

NOMAN=		noman

#		Generated at compile time (replaces supplied xdr_func.c)
SRCS+=		nfs_prot_x.c
CLEANFILES+=	nfs_prot_x.c

nfs_prot_x.c: ${NFS_PROT_X}
	${RPCCOM} -c -DWANT_NFS3 ${NFS_PROT_X} -o ${.TARGET}

install:

.include <bsd.lib.mk>
