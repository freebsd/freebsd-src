SRCS+=		conf.c host_syscall.S amd64_tramp.S elf64_freebsd.c

CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/ldscript.amd64
