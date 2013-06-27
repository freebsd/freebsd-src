
.PATH: ${.CURDIR}/../../vps

KMOD=	vps_account
SRCS=	vps_account.c opt_ddb.h opt_global.h opt_vps.h \
	vnode_if.h vps.h vps_account.h

.if !defined(KERNBUILDDIR)
opt_ddb.h:
	echo "#define DDB 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
