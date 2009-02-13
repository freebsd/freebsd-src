# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
