#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	kvm
CFLAGS+=-DLIBC_SCCS -I/sys
SRCS=	kvm.c kvm_${MACHINE}.c kvm_file.c kvm_getloadavg.c kvm_proc.c

MAN3=	kvm.0 kvm_geterr.0 kvm_getfiles.0 kvm_getloadavg.0 kvm_getprocs.0 \
	kvm_nlist.0 kvm_open.0 kvm_read.0

MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
MLINKS+=kvm_open.3 kvm_openfiles.3 kvm_open.3 kvm_close.3
MLINKS+=kvm_read.3 kvm_write.3

.include <bsd.lib.mk>
