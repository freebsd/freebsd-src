# $FreeBSD$

KMOD=	lomac
SRCS=	vnode_if.h kernel_lkm.c kernel_interface.c kernel_util.c \
	kernel_log.c kernel_mediate.c lomacfs_vfsops.c lomacfs_vnops.c \
	lomacfs_subr.c kernel_monitor.c kernel_mmap.c kernel_pipe.c \
	kernel_socket.c
NOMAN=	yes
SUBDIR+=	syscall_gate
SUBDIR+=	plm
CFLAGS+=	-I${.CURDIR}/../../contrib/lomac
CFLAGS+=	-DUSES_LOCKMGR		# necessary until VM uses sx locks
#CFLAGS+=	-DP_CAN_HOOKS	# There are no hooks for p_can*() ops in the
#				  kernel yet.  Until then, all subject-subject
#				  mediation is effectively "off".
#CFLAGS+=	-DLOMAC_DEBUG_LOOKUPSTATS	# relatively cheap
#CFLAGS+=	-DLOMAC_DEBUG_NODE_ALLOC	# pretty verbose
#CFLAGS+=	-DLOMAC_DEBUG_LINK		# pretty verbose
#CFLAGS+=	-DLOMAC_DEBUG_INCNAME		# can starve memory
#CFLAGS+=	-DLOMAC_DEBUG_INACTIVE		# extremely verbose
#CFLAGS+=	-DLOMAC_DEBUG_RECLAIM		# extremely verbose
#CFLAGS+=	-DLOMAC_DEBUG_LOOKUP		# extremely verbose
#CFLAGS+=	-DLOMAC_DEBUG_PIPE		# relatively cheap
#CFLAGS+=	-DLOMAC_DEBUG_RECWD		# only happens once

.PATH:	${.CURDIR}/../../contrib/lomac

load-all:	all
	@cd ${.CURDIR}/syscall_gate; ${MAKE} load
	@cd ${.CURDIR}/plm; ${MAKE} load
	@${MAKE} load

kldunload-binacts:
	-/sbin/kldunload linux
	-/sbin/kldunload svr4

start:	kldunload-binacts load-all

.include "Makefile.inc"
.include <bsd.kmod.mk>
