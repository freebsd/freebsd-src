#	$FreeBSD$
#

MSDOS=		${SRCTOP}/sys/fs/msdosfs
MSDOS_NEWFS=	${SRCTOP}/sbin/newfs_msdos

.PATH:	${SRCDIR}/msdos ${MSDOS} ${MSDOS_NEWFS}

CFLAGS+= -DMAKEFS -I${MSDOS} -I${MSDOS_NEWFS}

SRCS+= mkfs_msdos.c
SRCS+= msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c
SRCS+= msdosfs_vnops.c msdosfs_vfsops.c
