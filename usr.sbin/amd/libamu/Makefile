# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/amd/libamu \
       ${.CURDIR}/../../../contrib/amd/conf/transp \
       ${.CURDIR}/../../../contrib/amd/conf/mtab \
       ${.CURDIR}/../../../contrib/amd/conf/umount

LIB=	amu
INTERNALLIB=
SRCS=	hasmntopt.c misc_rpc.c mount_fs.c mtab.c nfs_prot_xdr.c \
	util.c wire.c xutil.c

#	These would be links created by the GNU-style configure
SRCS+=	transp_sockets.c mtab_bsd.c umount_bsd44.c

#		Generated at compile time (replaces supplied xdr_func.c)
SRCS+=		nfs_prot_x.c
CLEANFILES+=	nfs_prot_x.c

CFLAGS+= -I${.CURDIR}/../../../contrib/amd/libamu \
	 -I${DESTDIR}/usr/include/rpcsvc

nfs_prot_x.c: ${NFS_PROT_X}
	${RPCCOM} -c -C -DWANT_NFS3 ${NFS_PROT_X} -o ${.TARGET}

.include <bsd.lib.mk>
