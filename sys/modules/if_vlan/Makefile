# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	if_vlan
SRCS=	if_vlan.c opt_inet.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
