
.PATH: ${.CURDIR}/../../vps

KMOD=	vps_restore
SRCS=	vps_restore.c \
	vps.h vps_user.h vps_snapst.h vps_libdump.h \
	opt_ddb.h opt_vps.h opt_inet.h opt_inet6.h \
	vnode_if.h

.if !defined(KERNBUILDDIR)
opt_vps.h:
	echo "#define VPS 1" > ${.TARGET}

opt_ddb.h:
	echo "#define DDB 1" > ${.TARGET}

opt_inet.h:
        echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
        echo "#define INET6 1" > ${.TARGET}

.endif

.include <bsd.kmod.mk>
