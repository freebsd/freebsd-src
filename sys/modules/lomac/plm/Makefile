# $FreeBSD$

KMOD=	lomac_plm
SRCS=	vnode_if.h kernel_plm.c
CFLAGS+=	-I${.CURDIR}/../../../security/lomac

EXPORT_SYMS=	lomac_node_entry_root		\
		lomac_plm_init_lomacfs_vnode	\
		lomac_plm_initialized

.PATH:	${.CURDIR}/../../../security/lomac

.include "../Makefile.inc"
.include <bsd.kmod.mk>
