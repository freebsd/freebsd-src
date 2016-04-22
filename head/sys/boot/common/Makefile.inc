# $FreeBSD$

SRCS+=	boot.c commands.c console.c devopen.c interp.c 
SRCS+=	interp_backslash.c interp_parse.c ls.c misc.c 
SRCS+=	module.c panic.c

.if ${MACHINE} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SRCS+=	load_elf32.c load_elf32_obj.c reloc_elf32.c
SRCS+=	load_elf64.c load_elf64_obj.c reloc_elf64.c
.elif ${MACHINE} == "pc98"
SRCS+=	load_elf32.c load_elf32_obj.c reloc_elf32.c
.elif ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	load_elf64.c reloc_elf64.c
.elif ${MACHINE_CPUARCH} == "arm"
SRCS+=	load_elf32.c reloc_elf32.c
.elif ${MACHINE_CPUARCH} == "powerpc"
SRCS+=	load_elf32.c reloc_elf32.c
SRCS+=	load_elf64.c reloc_elf64.c
.elif ${MACHINE_CPUARCH} == "sparc64"
SRCS+=	load_elf64.c reloc_elf64.c
.elif ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
SRCS+= load_elf64.c reloc_elf64.c
.elif ${MACHINE} == "mips"
SRCS+=	load_elf32.c reloc_elf32.c
.endif

.if defined(LOADER_NET_SUPPORT)
SRCS+=	dev_net.c
.endif

.if !defined(LOADER_NO_DISK_SUPPORT)
SRCS+=	disk.c part.c
CFLAGS+= -DLOADER_DISK_SUPPORT
.if !defined(LOADER_NO_GPT_SUPPORT)
SRCS+=	crc32.c
CFLAGS+= -DLOADER_GPT_SUPPORT
.endif
.if !defined(LOADER_NO_MBR_SUPPORT)
CFLAGS+= -DLOADER_MBR_SUPPORT
.endif
.endif

.if defined(HAVE_BCACHE)
SRCS+=  bcache.c
.endif

.if defined(MD_IMAGE_SIZE)
CFLAGS+= -DMD_IMAGE_SIZE=${MD_IMAGE_SIZE}
SRCS+=	md.c
.endif

# Machine-independant ISA PnP
.if defined(HAVE_ISABUS)
SRCS+=	isapnp.c
.endif
.if defined(HAVE_PNP)
SRCS+=	pnp.c
.endif

# Forth interpreter
.if defined(BOOT_FORTH)
SRCS+=	interp_forth.c
.endif

.if defined(BOOT_PROMPT_123)
CFLAGS+=	-DBOOT_PROMPT_123
.endif

.if defined(LOADER_INSTALL_SUPPORT)
SRCS+=	install.c
CFLAGS+=-I${.CURDIR}/../../../../lib/libstand
.endif
