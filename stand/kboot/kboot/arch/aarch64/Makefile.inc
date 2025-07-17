SRCS+=		tramp.S exec.c load_addr.c fdt_arch.c

.PATH:	${BOOTSRC}/arm64/libarm64
CFLAGS+=-I${BOOTSRC}/arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}
