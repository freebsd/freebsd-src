# $Id: Makefile.inc,v 1.6 1998/09/30 19:38:26 peter Exp $

SRCS+=	boot.c commands.c console.c devopen.c interp.c interp_backslash.c
SRCS+=	interp_parse.c load_aout.c load_elf.c ls.c misc.c module.c panic.c

# Machine-independant ISA PnP
.if HAVE_ISABUS
SRCS+=	isapnp.c
.endif
.if HAVE_PNP
SRCS+=	pnp.c
.endif
