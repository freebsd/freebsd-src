SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/kinst \
		${SYSDIR}/cddl/dev/kinst/${MACHINE_CPUARCH}

KMOD=		kinst
SRCS=		kinst.c kinst_isa.c trampoline.c

CFLAGS+=	${OPENZFS_CFLAGS} \
		-I${SYSDIR}/cddl/dev/kinst \
		-I${SYSDIR}/cddl/dev/dtrace/x86 \
		-I${SYSDIR}/cddl/dev/kinst/${MACHINE_CPUARCH}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
CWARNFLAGS+=	${OPENZFS_CWARNFLAGS}
