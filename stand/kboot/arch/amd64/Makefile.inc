SRCS+=		host_syscall.S amd64_tramp.S elf64_freebsd.c load_addr.c

CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/ldscript.amd64
