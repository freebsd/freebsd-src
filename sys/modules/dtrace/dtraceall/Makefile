# $FreeBSD$

KMOD=		dtraceall
SRCS=		dtraceall.c opt_compat.h opt_nfs.h

CFLAGS+=	-I${.CURDIR}/../../..

.if !defined(KERNBUILDDIR)
.if ${MACHINE_ARCH} == "amd64"
opt_compat.h:
	echo "#define COMPAT_FREEBSD32 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
