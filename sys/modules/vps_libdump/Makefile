
.PATH: ${.CURDIR}/../../vps

KMOD=	vps_libdump
SRCS=	vps_libdump.c \
	vps_libdump.h \
	opt_ddb.h opt_vps.h opt_inet.h opt_inet6.h \
	vnode_if.h

.if !defined(KERNBUILDDIR)

.endif

.include <bsd.kmod.mk>
