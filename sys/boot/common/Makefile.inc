# $FreeBSD: src/sys/boot/common/Makefile.inc,v 1.9.2.1 2000/07/06 23:27:28 obrien Exp $

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
MAN5+=		../forth/loader.conf.5
MAN8+=          ../forth/loader.4th.8
.endif

MAN8+=          loader.8

