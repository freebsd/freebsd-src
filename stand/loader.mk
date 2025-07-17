.PATH: ${LDRSRC} ${BOOTSRC}/libsa

CFLAGS+=-I${LDRSRC}

SRCS+=	boot.c commands.c console.c devopen.c interp.c 
SRCS+=	interp_backslash.c interp_parse.c ls.c misc.c 
SRCS+=	modinfo.c
SRCS+=	module.c nvstore.c pnglite.c tslog.c

CFLAGS.module.c += -I$(SRCTOP)/sys/teken -I${SRCTOP}/contrib/pnglite

.PATH: ${SRCTOP}/contrib/pnglite
CFLAGS.pnglite.c+= -I${SRCTOP}/contrib/pnglite
CFLAGS.pnglite.c+= -DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib

.if ${MACHINE} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SRCS+=	load_elf32.c load_elf32_obj.c reloc_elf32.c
SRCS+=	load_elf64.c load_elf64_obj.c reloc_elf64.c
.elif ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	load_elf64.c reloc_elf64.c
.elif ${MACHINE_CPUARCH} == "arm"
SRCS+=	load_elf32.c reloc_elf32.c
.elif ${MACHINE_CPUARCH} == "powerpc"
SRCS+=	load_elf32.c reloc_elf32.c
SRCS+=	load_elf64.c reloc_elf64.c
SRCS+=	metadata.c
.elif ${MACHINE_CPUARCH} == "riscv"
SRCS+=	load_elf64.c reloc_elf64.c
SRCS+=	metadata.c
.endif

#
# LOADER_*_SUPPORT variables are used to subset the boot loader in the various
# configurations each platform supports. These are typically used to omit broken
# options, or to size optimize for space constrained instances. These are set in
# loader Makefiles (which include loader.mk) to control which subset of features
# are enabled. These cannot generally be set in src.conf since that would affect
# all loaders, but also not all loaders are setup for overrides like that and
# not all combinations of the following have been tested or even work. Sometimes
# non-default values won't work due to buggy support for that component being
# optional.
#
# LOADER_BZIP2_SUPPORT	Add support for bzip2 compressed files
# LOADER_CD9660_SUPPORT	Add support for iso9660 filesystems
# LOADER_DISK_SUPPORT	Adds support for disks and mounting filesystems on it
# LOADER_EXT2FS_SUPPORT	Add support for ext2 Linux filesystems
# LOADER_GPT_SUPPORT	Add support for GPT partitions
# LOADER_GZIP_SUPPORT	Add support for gzip compressed files
# LOADER_INSTALL_SUPPORT Add support for booting off of installl ISOs
# LOADER_MBR_SUPPORT	Add support for MBR partitions
# LOADER_MSDOS_SUPPORT	Add support for FAT filesystems
# LOADER_NET_SUPPORT	Adds networking support (useless w/o net drivers sometimes)
# LOADER_NFS_SUPPORT	Add NFS support
# LOADER_TFTP_SUPPORT	Add TFTP support
# LOADER_UFS_SUPPORT	Add support for UFS filesystems
# LOADER_ZFS_SUPPORT	Add support for ZFS filesystems
#

.if ${LOADER_DISK_SUPPORT:Uyes} == "yes"
CFLAGS.part.c+= -DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib
SRCS+=	disk.c part.c vdisk.c
.endif

.if ${LOADER_NET_SUPPORT:Uno} == "yes"
SRCS+= dev_net.c
.endif

.if defined(HAVE_BCACHE)
SRCS+=  bcache.c
.endif

.if defined(MD_IMAGE_SIZE)
CFLAGS+= -DMD_IMAGE_SIZE=${MD_IMAGE_SIZE}
SRCS+=	md.c
.else
CLEANFILES+=	md.o
.endif

# Machine-independent ISA PnP
.if defined(HAVE_ISABUS)
SRCS+=	isapnp.c
.endif
.if defined(HAVE_PNP)
SRCS+=	pnp.c
.endif

.if ${LOADER_INTERP} == "lua"
SRCS+=	interp_lua.c
.include "${BOOTSRC}/lua.mk"
LDR_INTERP=	${LIBLUA}
LDR_INTERP32=	${LIBLUA32}
CFLAGS.interp_lua.c= -DLUA_PATH=\"${LUAPATH}\" -I${FLUASRC}/modules
.elif ${LOADER_INTERP} == "4th"
SRCS+=	interp_forth.c
.include "${BOOTSRC}/ficl.mk"
LDR_INTERP=	${LIBFICL}
LDR_INTERP32=	${LIBFICL32}
.elif ${LOADER_INTERP} == "simp"
SRCS+=	interp_simple.c
.else
.error Unknown interpreter ${LOADER_INTERP}
.endif

.include "${BOOTSRC}/veriexec.mk"

.if defined(BOOT_PROMPT_123)
CFLAGS+=	-DBOOT_PROMPT_123
.endif

.if defined(LOADER_INSTALL_SUPPORT)
SRCS+=	install.c
.endif

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

# Partition support
.if ${LOADER_GPT_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_GPT_SUPPORT
.endif
.if ${LOADER_MBR_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_MBR_SUPPORT
.endif

.if ${HAVE_ZFS:Uno} == "yes"
CFLAGS+=	-DLOADER_ZFS_SUPPORT
CFLAGS+=	-I${ZFSSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/cddl/contrib/opensolaris/uts/common
SRCS+=		zfs_cmd.c
.endif

LIBFICL=	${BOOTOBJ}/ficl/libficl.a
.if ${MACHINE} == "i386"
LIBFICL32=	${LIBFICL}
.else
LIBFICL32=	${BOOTOBJ}/ficl32/libficl.a
.endif

LIBLUA=		${BOOTOBJ}/liblua/liblua.a
.if ${MACHINE} == "i386"
LIBLUA32=	${LIBLUA}
.else
LIBLUA32=	${BOOTOBJ}/liblua32/liblua.a
.endif

CLEANFILES+=	vers.c
VERSION_FILE?=	${.CURDIR}/version
.if ${MK_REPRODUCIBLE_BUILD} != no
REPRO_FLAG=	-r
.endif
vers.c: ${LDRSRC}/newvers.sh ${VERSION_FILE}
	sh ${LDRSRC}/newvers.sh ${REPRO_FLAG} ${VERSION_FILE} \
	    ${NEWVERSWHAT}

.if ${MK_LOADER_VERBOSE} != "no"
CFLAGS+=	-DELF_VERBOSE
.endif

# Each loader variant defines their own help filename. Optional or
# build-specific commands are included by augmenting HELP_FILES.
.if !defined(HELP_FILENAME)
.error Define HELP_FILENAME before including loader.mk
.endif

HELP_FILES+=	${LDRSRC}/help.common

CFLAGS+=	-DHELP_FILENAME=\"${HELP_FILENAME}\"
.if ${INSTALL_LOADER_HELP_FILE:Uyes} == "yes"
CLEANFILES+=	${HELP_FILENAME}
FILES+=		${HELP_FILENAME}
.endif

${HELP_FILENAME}: ${HELP_FILES}
	cat ${HELP_FILES} | awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}
