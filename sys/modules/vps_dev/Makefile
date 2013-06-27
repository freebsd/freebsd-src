
.PATH: ${.CURDIR}/../../vps

KMOD=	vps_dev
SRCS=	vps_dev.c vps_user.c opt_ddb.h opt_global.h opt_vps.h vps.h vps_user.h

.if !defined(KERNBUILDDIR)
opt_ddb.h:
	echo "#define DDB 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
