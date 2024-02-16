
LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
PIC=yes

# Note: -nostdlib needs to be added to LDFLAGS before including defs.mk
LDFLAGS+=	-nostdlib -Wl,-Bsymbolic

.include <bsd.init.mk>

SHLIB_NAME=	userboot_${LOADER_INTERP}.so
STRIP=
LIBDIR=		/boot

.PATH:		${.CURDIR}/../userboot
SRCS=		autoload.c
SRCS+=		bcache.c
SRCS+=		biossmap.c
SRCS+=		bootinfo.c
SRCS+=		bootinfo32.c
SRCS+=		bootinfo64.c
SRCS+=		conf.c
SRCS+=		copy.c
SRCS+=		devicename.c
SRCS+=		elf32_freebsd.c
SRCS+=		elf64_freebsd.c
SRCS+=		host.c
SRCS+=		main.c
SRCS+=		userboot_cons.c
SRCS+=		userboot_disk.c
SRCS+=		vers.c

CFLAGS+=	-Wall
CFLAGS+=	-I${BOOTSRC}/userboot

.if ${MK_LOADER_ZFS} != "no"
CFLAGS.main.c+=	-I${BOOTSRC}/libsa/zfs
CFLAGS.main.c+=	-I${SYSDIR}/contrib/openzfs/include
CFLAGS.main.c+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
.endif

CWARNFLAGS.main.c += -Wno-implicit-function-declaration

NEWVERSWHAT=	"User boot ${LOADER_INTERP}" ${MACHINE_CPUARCH}
VERSION_FILE=	${.CURDIR}/../userboot/version

.if ${LOADER_INTERP} == ${LOADER_DEFAULT_INTERP}
LINKS+=		${BINDIR}/${SHLIB_NAME} ${BINDIR}/userboot.so
.endif

.if ${MK_LOADER_ZFS} != "no"
CFLAGS+=	-DUSERBOOT_ZFS_SUPPORT
HAVE_ZFS=yes
.endif

HELP_FILENAME=	loader.help.userboot

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I.
DPADD+=		${LDR_INTERP} ${LIBSA}
LDADD+=		${LDR_INTERP} ${LIBSA}

.include <bsd.lib.mk>
