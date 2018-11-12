# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/cloudabi64
.PATH: ${.CURDIR}/../../${MACHINE}/cloudabi64

KMOD=	cloudabi64
SRCS=	cloudabi64_fd.c cloudabi64_module.c cloudabi64_poll.c \
	cloudabi64_sock.c cloudabi64_syscalls.c cloudabi64_sysent.c \
	cloudabi64_sysvec.c cloudabi64_thread.c

.include <bsd.kmod.mk>
