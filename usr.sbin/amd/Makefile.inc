# ex:ts=8
#
# $FreeBSD: src/usr.sbin/amd/Makefile.inc,v 1.5 1999/08/28 01:15:12 peter Exp $
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998.
#	Portions derived from
#	$NetBSD: Makefile.inc,v 1.10 1998/08/08 22:33:27 christos Exp $
#	Portions derived from amd/libamu/Makefile
#	$NetBSD: Makefile,v 1.8 1998/08/08 22:33:37 christos Exp $
#

#INCGEN!=	cd ${.CURDIR}/../include; \
#		printf 'xwhere: .MAKE\n\t@echo \$${.OBJDIR}\n' | ${MAKE} -Bs -f-

#CFLAGS+=	-I${INCGEN}
CFLAGS+=	-I. -I${.CURDIR} 
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/include
CFLAGS+=	-I${.CURDIR}/../../../contrib/amd
CFLAGS+=	-DHAVE_CONFIG_H

LIBAMUDIR!=	cd ${.CURDIR}/../libamu; \
		printf 'xwhere: .MAKE\n\t@echo \$${.OBJDIR}\n' | ${MAKE} -Bs -f-
LIBAMU=		${LIBAMUDIR}/libamu.a

.if !defined(INFO)

SRCS+=		config_local.h
CLEANFILES+=	config_local.h
config_local.h: ${.CURDIR}/../include/newvers.sh
	@rm -f ${.TARGET}
	sh ${.ALLSRC} > ${.TARGET}


RPCCOM=		rpcgen
MOUNT_X=	${DESTDIR}/usr/include/rpcsvc/mount.x
NFS_PROT_X=	${DESTDIR}/usr/include/rpcsvc/nfs_prot.x
#		These are generated at compile time
SRCS+=		mount.h nfs_prot.h
CLEANFILES+=	mount.h nfs_prot.h

mount.h: ${MOUNT_X}
	${RPCCOM} -h -C -DWANT_NFS3 ${MOUNT_X} -o ${.TARGET}

nfs_prot.h: ${NFS_PROT_X}
	${RPCCOM} -h -C -DWANT_NFS3 ${NFS_PROT_X} -o ${.TARGET}

.endif

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
