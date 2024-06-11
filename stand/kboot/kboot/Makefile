
LOADER_DISK_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_ZFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	no
LOADER_NFS_SUPPORT?=	no
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no

.include <bsd.init.mk>

PROG=		loader.kboot
NEWVERSWHAT=	"kboot loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=	\
		bootinfo.c \
		conf.c \
		hostcons.c \
		hostdisk.c \
		hostfs.c \
		init.c \
		kbootfdt.c \
		main.c \
		seg.c \
		util.c \
		vers.c

.if ${MK_LOADER_ZFS} != "no"
CFLAGS+=	-I${ZFSSRC}
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
HAVE_ZFS=yes
.endif

HELP_FILENAME=	loader.help.kboot

.include	"${BOOTSRC}/fdt.mk"

# We share bootinfo.c with efi
.PATH:		${BOOTSRC}/efi/loader

# Note: Since we're producing a userland binary, we key off of MACHINE_ARCH
# instead of the more normal MACHINE since the changes between different flavors
# of MACHINE_ARCH are large enough in Linux that it's easier that way.
.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

# Bring in our ldscript
LDFLAGS=	-nostdlib -static -T ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}.ldscript

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}

CFLAGS+=	-Wall

DPADD=		${LDR_INTERP} ${LIBOFW} ${LIBSAFDT} ${LIBSA} ${LIBKBOOT}
LDADD=		${LDR_INTERP} ${LIBOFW} ${LIBSAFDT} ${LIBSA} ${LIBKBOOT}

# Add our own lua bindings until they are universal
.if ${LOADER_INTERP} == "lua"
SRCS+=		\
		lua_bindings.c
CFLAGS.lua_bindings.c+=  -I${FLUALIB}/libhash
.endif

.include <bsd.prog.mk>
