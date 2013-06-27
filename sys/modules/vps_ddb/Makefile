
.PATH: ${.CURDIR}/../../vps

KMOD=	vps_ddb
SRCS=	vps_ddb.c opt_ddb.h opt_inet.h opt_inet6.h vps.h vps_user.h vps_snapst.h

.if !defined(KERNBUILDDIR)
opt_ddb.h:
	echo "#define DDB 1" > ${.TARGET}

opt_inet.h:
        echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
        echo "#define INET6 1" > ${.TARGET}

.endif

.include <bsd.kmod.mk>
