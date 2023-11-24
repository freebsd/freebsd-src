SRCS+=		host_syscall.S tramp.S exec.c load_addr.c fdt_arch.c

.PATH:	${BOOTSRC}/arm64/libarm64
CFLAGS+=-I${BOOTSRC}/arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/ldscript.${MACHINE_ARCH}
