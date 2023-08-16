
SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/contrib/opensolaris/uts/common/dtrace

KMOD=		fasttrap
SRCS=		fasttrap.c fasttrap_isa.c
SRCS+=		vnode_if.h  opt_global.h

CFLAGS+= 	-include ${.OBJDIR}/opt_global.h
CFLAGS+=	${OPENZFS_CFLAGS}

.if ${MACHINE_CPUARCH} == "amd64" ||  ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-I${SYSDIR}/cddl/contrib/opensolaris/uts/intel
.PATH:		${SYSDIR}/cddl/contrib/opensolaris/uts/intel/dtrace
.elif ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-I${SYSDIR}/cddl/contrib/opensolaris/uts/powerpc
.PATH:		${SYSDIR}/cddl/contrib/opensolaris/uts/powerpc/dtrace
.endif

.PATH:		${SYSDIR}/contrib/openzfs/module/unicode
SRCS+=		u8_textprep.c

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h

CWARNFLAGS+=	-Wno-cast-qual
CWARNFLAGS+=	-Wno-unused
