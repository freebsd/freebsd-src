CFLAGS+=	-mcpu=powerpc64

SRCS+=		ppc64_elf_freebsd.c kerneltramp.S load_addr.c fdt_arch.c

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/ldscript.powerpc

MK_PIE=		no
