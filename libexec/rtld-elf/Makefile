#
#	$Id: Makefile,v 1.1.1.1 1998/03/07 19:24:35 jdp Exp $
#

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c map_object.c malloc.c xmalloc.c debug.c
NOMAN=		true
CFLAGS+=	-elf -fpic -DFREEBSD_ELF
CFLAGS+=	-Wall
LDFLAGS+=	-elf -nostdlib -Wl,-Bshareable,-Bsymbolic
LDADD+=		-lc_pic

.PATH: ${.CURDIR}/${MACHINE}

.include <bsd.prog.mk>
