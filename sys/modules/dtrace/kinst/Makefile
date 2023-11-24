SYSDIR?=	${SRCTOP}/sys

KMOD=		kinst
SRCS=		kinst.c kinst_isa.c trampoline.c

CFLAGS+=	${OPENZFS_CFLAGS} \
		-I${SYSDIR}/cddl/dev/kinst \
		-I${SYSDIR}/cddl/dev/kinst/${MACHINE_CPUARCH}

.PATH:		${SYSDIR}/cddl/dev/kinst \
		${SYSDIR}/cddl/dev/kinst/${MACHINE_CPUARCH}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-I${SYSDIR}/cddl/dev/dtrace/x86
.PATH:		${SYSDIR}/cddl/dev/dtrace/x86
.else
CFLAGS+=	-I${SYSDIR}/cddl/dev/dtrace/${MACHINE_CPUARCH}
.PATH:		${SYSDIR}/cddl/dev/dtrace/${MACHINE_CPUARCH}
.endif

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
