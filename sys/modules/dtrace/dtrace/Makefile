# $FreeBSD$

ARCHDIR=	${MACHINE_CPUARCH}

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common/dtrace
.PATH: ${.CURDIR}/../../../cddl/compat/opensolaris/kern
.PATH: ${.CURDIR}/../../../cddl/kern
.PATH: ${.CURDIR}/../../../cddl/dev/dtrace
.PATH: ${.CURDIR}/../../../cddl/dev/dtrace/${ARCHDIR}

KMOD=		dtrace
SRCS=		dtrace.c \
		dtrace_asm.S \
		dtrace_subr.c

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SRCS+=		dis_tables.c \
		instr_size.c
CFLAGS+=	-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/intel
.endif

SRCS+=		bus_if.h device_if.h vnode_if.h

# Needed for dtrace_asm.S
SRCS+=		assym.s

# These are needed for assym.s
SRCS+=		opt_compat.h opt_kstack_pages.h opt_nfs.h opt_hwpmc_hooks.h

#This is needed for dtrace.c
SRCS += 	opensolaris_taskq.c

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=		opt_apic.h
.endif

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/dev/dtrace \
		-I${.CURDIR}/../../../cddl/dev/dtrace/${ARCHDIR} \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../.. -DDIS_MEM

CFLAGS+=	-DSMP -DDEBUG

EXPORT_SYMS=	dtrace_register \
		dtrace_unregister \
		dtrace_probe_lookup

dtrace_asm.o:  assym.s

.include <bsd.kmod.mk>

CWARNFLAGS+=	-Wno-parentheses
CWARNFLAGS+=	-Wno-uninitialized
