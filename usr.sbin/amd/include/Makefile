# ex:ts=8
#
# $FreeBSD$
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998.
#	Portions derived from amd/libamu/Makefile
#	$NetBSD: Makefile,v 1.8 1998/08/08 22:33:37 christos Exp $
#

SRCS=		config_local.h mount.h nfs_prot.h
CLEANFILES=	${SRCS}

config_local.h: newvers.sh
	@rm -f ${.TARGET}
	sh ${.ALLSRC} > ${.TARGET}

mount.h: ${MOUNT_X}
	${RPCCOM} -h -C -DWANT_NFS3 ${MOUNT_X} -o ${.TARGET}

nfs_prot.h: ${NFS_PROT_X}
	${RPCCOM} -h -C -DWANT_NFS3 ${NFS_PROT_X} -o ${.TARGET}

.include <bsd.prog.mk>
