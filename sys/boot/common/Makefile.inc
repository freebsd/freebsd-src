# $FreeBSD: src/sys/boot/common/Makefile.inc,v 1.8.2.1 1999/08/29 16:20:23 peter Exp $

SRCS+=	bcache.c boot.c commands.c console.c devopen.c interp.c 
SRCS+=	interp_backslash.c interp_parse.c load_aout.c load_elf.c ls.c misc.c 
SRCS+=	module.c panic.c

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
.endif
