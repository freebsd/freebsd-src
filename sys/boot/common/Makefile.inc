# $FreeBSD$

SRCS+=	bcache.c boot.c commands.c console.c devopen.c interp.c 
SRCS+=	interp_backslash.c interp_parse.c ls.c misc.c 
SRCS+=	module.c panic.c

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SRCS+=	load_elf32.c load_elf64.c
.endif
.if ${MACHINE_ARCH} == "powerpc"
SRCS+=	load_elf32.c
.endif
.if ${MACHINE_ARCH} == "sparc64" || ${MACHINE_ARCH} == "ia64" || ${MACHINE_ARCH} == "alpha"
SRCS+=	load_elf64.c
.endif

.if defined(LOADER_NET_SUPPORT)
SRCS+=	dev_net.c
.endif

# Machine-independant ISA PnP
.if HAVE_ISABUS
SRCS+=	isapnp.c
.endif
.if HAVE_PNP
SRCS+=	pnp.c
.endif

# Forth interpreter
.if BOOT_FORTH
SRCS+=	interp_forth.c
MAN+=	../forth/loader.conf.5
MAN+=	../forth/loader.4th.8
.endif

MAN+=	loader.8
