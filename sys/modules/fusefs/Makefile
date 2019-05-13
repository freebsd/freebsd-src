# $FreeBSD$

.PATH: ${SRCTOP}/sys/fs/fuse

KMOD=	fusefs
SRCS=	vnode_if.h \
	fuse_node.c fuse_io.c fuse_device.c fuse_ipc.c fuse_file.c \
	fuse_vfsops.c fuse_vnops.c fuse_internal.c fuse_main.c

# Symlink for backwards compatibility with systems installed at 12.0 or older
.if ${MACHINE_CPUARCH} != "powerpc"
SYMLINKS=	${KMOD}.ko ${KMODDIR}/fuse.ko
.else
# Some PPC systems use msdosfs for /boot, which can't handle links or symlinks
afterinstall: alias alias_debug
alias: .PHONY
	${INSTALL} -T release -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}/fuse.ko
.if defined(DEBUG_FLAGS) && !defined(INSTALL_NODEBUG) && "${MK_KERNEL_SYMBOLS}" != "no"
alias_debug: .PHONY
	${INSTALL} -T debug -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG}.debug \
	    ${DESTDIR}${KERN_DEBUGDIR}${KMODDIR}/fuse.ko
.else
alias_debug: .PHONY
.endif
.endif

.include <bsd.kmod.mk>
