.PATH: ${SRCTOP}/sys/compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h opt_inet.h \
	linprocfs.c

.if !defined(KERNBUILDDIR)
.warning Building Linuxulator outside of a kernel does not make sense
.endif

.include <bsd.kmod.mk>
