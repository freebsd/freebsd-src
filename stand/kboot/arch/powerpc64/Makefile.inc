CFLAGS+=	-mcpu=powerpc64

SRCS+=		ppc64_elf_freebsd.c host_syscall.S kerneltramp.S
SRCS+=		ucmpdi2.c

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/ldscript.powerpc

MK_PIE=		no
