# $FreeBSD: src/sys/modules/dtrace/dtraceall/Makefile,v 1.1.4.3.2.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=		dtraceall
SRCS=		dtraceall.c opt_compat.h

CFLAGS+=	-I${.CURDIR}/../../..

.if !defined(KERNBUILDDIR)
opt_compat.h:
.if ${MACHINE_ARCH} == "amd64"
	echo "#define COMPAT_FREEBSD32 1" >> ${.TARGET}
.else
	echo -n >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
