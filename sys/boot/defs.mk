# $FreeBSD$

.include <src.opts.mk>

.if !defined(__BOOT_DEFS_MK__)
__BOOT_DEFS_MK__=${MFILE}

BOOTSRC=	${SRCTOP}/sys/boot
EFISRC=		${BOOTSRC}/efi
EFIINC=		${EFISRC}/include
EFIINCMD=	${EFIINC}/${MACHINE}
FDTSRC=		${BOOTSRC}/fdt
FICLSRC=	${BOOTSRC}/ficl
LDRSRC=		${BOOTSRC}/common
SASRC=		${BOOTSRC}/libsa
SYSDIR=		${SRCTOP}/sys
UBOOTSRC=	${BOOTSRC}/uboot
ZFSSRC=		${BOOTSRC}/zfs

BOOTOBJ=	${OBJTOP}/sys/boot

# BINDIR is where we install
BINDIR?=	/boot

# NB: The makefiles depend on these being empty when we don't build forth.
.if ${MK_FORTH} != "no"
LIBFICL=	${BOOTOBJ}/ficl/libficl.a
.if ${MACHINE} == "i386"
LIBFICL32=	${LIBFICL}
.else
LIBFICL32=	${BOOTOBJ}/ficl32/libficl.a
.endif
.endif
LIBSA=		${BOOTOBJ}/libsa/libsa.a
.if ${MACHINE} == "i386"
LIBSA32=	${LIBSA}
.else
LIBSA32=	${BOOTOBJ}/libsa32/libsa32.a
.endif

# Standard options:

# Filesystem support
.if ${LOADER_CD9660_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_CD9660_SUPPORT
.endif
.if ${LOADER_EXT2FS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_EXT2FS_SUPPORT
.endif
.if ${LOADER_MSDOS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_MSDOS_SUPPORT
.endif
.if ${LOADER_NANDFS_SUPPORT:U${MK_NAND}} == "yes"
CFLAGS+=	-DLOADER_NANDFS_SUPPORT
.endif
.if ${LOADER_UFS_SUPPORT:Uyes} == "yes"
CFLAGS+=	-DLOADER_UFS_SUPPORT
.endif

# Compression
.if ${LOADER_GZIP_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if ${LOADER_BZIP2_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif

# Network related things
.if ${LOADER_NET_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_NET_SUPPORT
.endif
.if ${LOADER_NFS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif
.if ${LOADER_TFTP_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.endif

# Disk and partition support
.if ${LOADER_DISK_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_DISK_SUPPORT
.if ${LOADER_GPT_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_GPT_SUPPORT
.endif
.if ${LOADER_MBR_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_MBR_SUPPORT
.endif

# GELI Support, with backward compat hooks
.if defined(HAVE_GELI)
.if defined(LOADER_NO_GELI_SUPPORT)
MK_LOADER_GELI=no
.warning "Please move from LOADER_NO_GELI_SUPPORT to WITHOUT_LOADER_GELI"
.endif
.if defined(LOADER_GELI_SUPPORT)
MK_LOADER_GELI=yes
.warning "Please move from LOADER_GELI_SUPPORT to WITH_LOADER_GELI"
.endif
.if ${MK_LOADER_GELI} == "yes"
CFLAGS+=	-DLOADER_GELI_SUPPORT
CFLAGS+=	-I${BOOTSRC}/geli
LIBGELIBOOT=	${BOOTOBJ}/geli/libgeliboot.a
.endif
.endif
.endif

CFLAGS+=	-I${SYSDIR}

# All PowerPC builds are 32 bit. We have no 64-bit loaders on powerpc
# or powerpc64.
.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc
.endif

# For amd64, there's a bit of mixed bag. Some of the tree (i386, lib*32) is
# build 32-bit and some 64-bit (lib*, efi). Centralize all the 32-bit magic here
# and activate it when DO32 is explicitly defined to be 1.
.if ${MACHINE_ARCH} == "amd64" && ${DO32:U0} == 1
CFLAGS+=	-m32 -mcpu=i386
# LD_FLAGS is passed directly to ${LD}, not via ${CC}:
LD_FLAGS+=	-m elf_i386_fbsd
AFLAGS+=	--32
.endif

_ILINKS=machine
.if ${MACHINE} != ${MACHINE_CPUARCH} && ${MACHINE} != "arm64"
_ILINKS+=${MACHINE_CPUARCH}
.endif
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_ILINKS+=x86
.endif
CLEANFILES+=${_ILINKS}

all: ${PROG}

beforedepend: ${_ILINKS}
beforebuild: ${_ILINKS}

# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${OBJS}:       ${_link}
.endif
.endfor

.NOPATH: ${_ILINKS}

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		if [ ${DO32:U0} -eq 0 ]; then \
			path=${SYSDIR}/${MACHINE}/include ; \
		else \
			path=${SYSDIR}/${MACHINE:C/amd64/i386/}/include ; \
		fi ;; \
	*) \
		path=${SYSDIR}/${.TARGET:T}/include ;; \
	esac ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET:T} "->" $$path ; \
	ln -fhs $$path ${.TARGET:T}

.endif # __BOOT_DEFS_MK__
