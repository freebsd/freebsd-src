# $FreeBSD$

SRCS+=	bcache.c boot.c commands.c console.c devopen.c interp.c 
SRCS+=	interp_backslash.c interp_parse.c load_elf.c ls.c misc.c 
SRCS+=	module.c panic.c

.if defined(LOADER_AOUT_SUPPORT)
SRCS+=	load_aout.c
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

