#
#	$Id: Makefile,v 1.2 1998/04/30 07:47:58 dfr Exp $
#

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c map_object.c malloc.c xmalloc.c debug.c
NOMAN=		true
CFLAGS+=	-elf -fpic -DFREEBSD_ELF
CFLAGS+=	-Wall
LDFLAGS+=	-elf -nostdlib -Wl,-Bshareable,-Bsymbolic
LDADD+=		-lc_pic
# Atomic installation with "-C" is very important for this program.
INSTALLFLAGS+=	-C

.PATH: ${.CURDIR}/${MACHINE}

.include <bsd.prog.mk>
