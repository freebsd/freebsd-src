# $FreeBSD$
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998.
#	Portions derived from
#	$NetBSD: Makefile.inc,v 1.10 1998/08/08 22:33:27 christos Exp $
#	Portions derived from amd/libamu/Makefile
#	$NetBSD: Makefile,v 1.8 1998/08/08 22:33:37 christos Exp $

CFLAGS += -I. -I${.CURDIR} 
CFLAGS += -I${COMMONSTAGEDIR}/usr/include/amd
CFLAGS += -I${COMMONSHAREDSTAGEDIR}/usr/include/amd
CFLAGS += -I${BSDSRCTOP}/usr.sbin/amd/include
CFLAGS += -I${BSDSRCTOP}/contrib/amd/include
CFLAGS += -I${BSDSRCTOP}/contrib/amd
CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -DYES_HESIOD

CFLAGS += -DHOST_CPU=\"${MACHINE_ARCH}\" -DHOST_ARCH=\"${MACHINE_ARCH}\"

.if exists(${COMMONSTAGEDIR}/usr/include/rpcsvc/mount.x)
MOUNT_X = ${COMMONSTAGEDIR}/usr/include/rpcsvc/mount.x
.else
MOUNT_X = ${COMMONSHAREDSTAGEDIR}/usr/include/rpcsvc/mount.x
.endif

.if exists(${COMMONSTAGEDIR}/usr/include/rpcsvc/nfs_prot.x)
NFS_PROT_X = ${COMMONSTAGEDIR}/usr/include/rpcsvc/nfs_prot.x
.else
NFS_PROT_X = ${COMMONSHAREDSTAGEDIR}/usr/include/rpcsvc/nfs_prot.x
.endif

.if exists(${.CURDIR}/../../Buildfile.inc)
.include "${.CURDIR}/../../Buildfile.inc"
.endif
