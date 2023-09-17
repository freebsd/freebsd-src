
.PATH: ${SRCTOP}/sys/compat/linux
.if ${MACHINE_CPUARCH} == "amd64"
.PATH: ${SRCTOP}/sys/x86/linux
.endif

KMOD=	linux_common
SRCS=	linux_common.c linux_mib.c linux_mmap.c linux_util.c linux_emul.c \
	linux_dummy.c linux_errno.c linux_netlink.c \
	linux.c device_if.h vnode_if.h bus_if.h opt_inet6.h opt_inet.h \
	opt_netlink.h

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	linux_x86.c linux_vdso_selector_x86.c
.endif

EXPORT_SYMS=
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_use_real_ifname

.if !defined(KERNBUILDDIR)
.warning Building Linuxulator outside of a kernel does not make sense
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
