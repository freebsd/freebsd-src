# $FreeBSD$

MAN=

LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no

.include <bsd.init.mk>

MK_SSP=		no

SHLIB_NAME=	userboot.so
MK_CTF=		no
STRIP=
LIBDIR=		/boot

SRCS=		autoload.c
SRCS+=		bcache.c
SRCS+=		biossmap.c
SRCS+=		bootinfo.c
SRCS+=		bootinfo32.c
SRCS+=		bootinfo64.c
SRCS+=		conf.c
SRCS+=		console.c
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

CWARNFLAGS.main.c += -Wno-implicit-function-declaration

LDFLAGS+=	-nostdlib -Wl,-Bsymbolic

NEWVERSWHAT=	"User boot" ${MACHINE_CPUARCH}

.if ${MK_ZFS} != "no"
CFLAGS+=	-DUSERBOOT_ZFS_SUPPORT
LIBZFSBOOT=	${BOOTOBJ}/zfs/libzfsboot.a
.endif

# Always add MI sources 
HELP_FILES=	# Disable
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I.
DPADD+=		${LIBFICL} ${LIBZFSBOOT} ${LIBSA} 
LDADD+=		${LIBFICL} ${LIBZFSBOOT} ${LIBSA}

.include <bsd.lib.mk>
