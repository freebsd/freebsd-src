.include <bsd.init.mk>

LIB=	kboot
WARNS?= 4

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}

SRCS=	crt1.c \
	host_syscall.S \
	host_syscalls.c \
	termios.c

.sinclude "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

.include <bsd.lib.mk>
