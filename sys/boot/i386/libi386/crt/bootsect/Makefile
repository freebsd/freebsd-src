#
# Build a startup module
#

# Disk buffer size in sectors
CFLAGS+= -DDISKBUFSIZE=18
# Stack size in bytes
CFLAGS+= -DSTACKSIZE=0x1000

PROG=		bscrt.o
LDFLAGS=	-static -nostartfiles -nostdlib -Xlinker -r
NOMAN=		yes

# Bootblock-specific sources
SRCS=		start_bootsect.S fraglist.S bootsectmain.c 

# Includes for libsa support routines XXX fix path!
CFLAGS+=	-I${.CURDIR}/../../../../../whole/lib/libstand

# Common items across all startups
.PATH:		${.CURDIR}/..
CFLAGS+=	-I${.CURDIR}/..
SRCS+=		biosdisk_ll.c bios_disk.S buffers.S

.include <bsd.prog.mk>
