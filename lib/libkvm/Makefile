#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

LIB=	kvm
SHLIBDIR?= /lib
CFLAGS+=-DLIBC_SCCS -I${.CURDIR}

.if exists(${.CURDIR}/kvm_${MACHINE_ARCH}.c)
KVM_ARCH=${MACHINE_ARCH}
.else
KVM_ARCH=${MACHINE_CPUARCH}
.endif

WARNS?=	3

SRCS=	kvm.c kvm_${KVM_ARCH}.c kvm_cptime.c kvm_file.c kvm_getloadavg.c \
	kvm_getswapinfo.c kvm_pcpu.c kvm_proc.c kvm_vnet.c
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386" || \
    ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "mips"
SRCS+=	kvm_minidump_${KVM_ARCH}.c
.endif
INCS=	kvm.h

MAN=	kvm.3 kvm_getcptime.3 kvm_geterr.3 kvm_getfiles.3 kvm_getloadavg.3 \
	kvm_getpcpu.3 kvm_getprocs.3 kvm_getswapinfo.3 kvm_nlist.3 kvm_open.3 \
	kvm_read.3

MLINKS+=kvm_getpcpu.3 kvm_getmaxcpu.3 \
	kvm_getpcpu.3 kvm_dpcpu_setcpu.3 \
	kvm_getpcpu.3 kvm_read_zpcpu.3 \
	kvm_getpcpu.3 kvm_counter_u64_fetch.3
MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
MLINKS+=kvm_open.3 kvm_close.3 kvm_open.3 kvm_openfiles.3
MLINKS+=kvm_read.3 kvm_write.3

.include <bsd.lib.mk>
