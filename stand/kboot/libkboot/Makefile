.include <bsd.init.mk>

LIB=	kboot
WARNS?= 4

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}
CFLAGS+=-I${LDRSRC}

SRCS=	crt1.c
SRCS+=	dfk.c
SRCS+=	host_syscall.S
SRCS+=	host_syscalls.c
SRCS+=	seg.c
SRCS+=	termios.c
SRCS+=	util.c

.if ${MACHINE_ARCH} != "powerpc64"
SRCS+=	efi.c
.endif

.sinclude "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

.include <bsd.lib.mk>
