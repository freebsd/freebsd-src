# $FreeBSD$

ARCHDIR=	${MACHINE_ARCH}

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common/dtrace
.PATH: ${.CURDIR}/../../../cddl/kern
.PATH: ${.CURDIR}/../../../cddl/dev/dtrace
.PATH: ${.CURDIR}/../../../cddl/dev/dtrace/${ARCHDIR}

KMOD=		dtrace
SRCS=		dtrace.c \
		dtrace_asm.S \
		dtrace_subr.c

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=		dis_tables.c \
		instr_size.c
CFLAGS+=	-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/intel
.endif

SRCS+=		bus_if.h device_if.h vnode_if.h

# Needed for dtrace_asm.S
SRCS+=		assym.s

# These are needed for assym.s
SRCS+=		opt_compat.h opt_kstack_pages.h opt_nfs.h opt_hwpmc_hooks.h

.if ${MACHINE_ARCH} == "i386"
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

.include <bsd.kmod.mk>
