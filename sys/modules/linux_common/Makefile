# $FreeBSD$

.PATH: ${SRCTOP}/sys/compat/linux

KMOD=	linux_common
SRCS=	linux_common.c linux_mib.c linux_mmap.c linux_util.c linux_emul.c \
	linux_dummy.c linux_errno.c \
	linux.c device_if.h vnode_if.h bus_if.h opt_inet6.h

EXPORT_SYMS=
EXPORT_SYMS+=	linux_emul_path
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_use_real_ifname

.if !defined(KERNBUILDDIR)
.warning Building Linuxulator outside of a kernel does not make sense
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
