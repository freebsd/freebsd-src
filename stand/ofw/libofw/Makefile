# $FreeBSD$

.include <bsd.init.mk>

LIB=		ofw

SRCS=	devicename.c elf_freebsd.c ofw_console.c ofw_copy.c ofw_disk.c \
	ofw_memory.c ofw_module.c ofw_net.c ofw_reboot.c \
	ofw_time.c openfirm.c
.PATH:	${ZFSSRC}
SRCS+=  devicename_stubs.c

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${LDRSRC}

.if ${MACHINE_CPUARCH} == "powerpc"
SRCS+=		ppc64_elf_freebsd.c
.endif

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.include <bsd.lib.mk>
