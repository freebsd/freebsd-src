SRCDIR:=${.PARSEDIR:tA}

.include <src.opts.mk>

PROG=	makefs

CFLAGS+=-I${SRCDIR}

SRCS=	cd9660.c \
	ffs.c \
	makefs.c \
	msdos.c \
	mtree.c \
	walk.c
MAN=	makefs.8

NO_WCAST_ALIGN=

.if ${MK_ZFS} != "no"
SRCS+=	zfs.c
CFLAGS+=-I${SRCDIR}/zfs \
	-I${SRCTOP}/stand/libsa \
	-I${SRCTOP}/sys/cddl/boot

CFLAGS+=	-DHAVE_ZFS

.include "${SRCDIR}/zfs/Makefile.inc"
.endif

.include "${SRCDIR}/cd9660/Makefile.inc"
.include "${SRCDIR}/ffs/Makefile.inc"
.include "${SRCDIR}/msdos/Makefile.inc"

CFLAGS+=-DHAVE_STRUCT_STAT_ST_FLAGS=1

.PATH: ${SRCTOP}/contrib/mtree
CFLAGS+=-I${SRCTOP}/contrib/mtree
SRCS+=	getid.c misc.c spec.c

.PATH: ${SRCTOP}/contrib/mknod
CFLAGS+=-I${SRCTOP}/contrib/mknod
SRCS+=	pack_dev.c

CFLAGS+=	-I${SRCTOP}/lib/libnetbsd
LIBADD=		netbsd util sbuf

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

# cd9660_generate_path_table adds a global variable to an on-stack
# TAILQ which temporarily stores a pointer to the on-stack TAILQ head
# in the global
CWARNFLAGS.cd9660.c+= ${NO_WDANGLING_POINTER}

.include <bsd.prog.mk>
