#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	scsi
#CFLAGS+=-DLIBC_SCCS -I/sys
SRCS=	scsi.c

MAN3=	scsi.3

#MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
#MLINKS+=kvm_open.3 kvm_openfiles.3 kvm_open.3 kvm_close.3
#MLINKS+=kvm_read.3 kvm_write.3

beforeinstall:
	-cd ${.CURDIR}; cmp -s scsi.h ${DESTDIR}/usr/include/scsi.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 scsi.h \
	    ${DESTDIR}/usr/include


.include <bsd.lib.mk>
