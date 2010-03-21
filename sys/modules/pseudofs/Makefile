# $FreeBSD: src/sys/modules/pseudofs/Makefile,v 1.4.22.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/pseudofs

KMOD=	pseudofs
SRCS=	opt_mac.h \
	opt_pseudofs.h \
	vnode_if.h \
	pseudofs.c \
	pseudofs_fileno.c \
	pseudofs_vncache.c \
	pseudofs_vnops.c

EXPORT_SYMS=	pfs_mount	\
		pfs_unmount	\
		pfs_root	\
		pfs_statfs	\
		pfs_init	\
		pfs_uninit	\
		pfs_create_dir	\
		pfs_create_file	\
		pfs_create_link	\
		pfs_disable	\
		pfs_enable	\
		pfs_destroy

.include <bsd.kmod.mk>
