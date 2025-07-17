CFLAGS+=	-mcpu=powerpc64

SRCS+=		ppc64_elf_freebsd.c kerneltramp.S load_addr.c fdt_arch.c

MK_PIE=		no
