# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

KMOD=   ipdivert
SRCS=   ip_divert.c opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
