# $FreeBSD$

MAN=

.include <src.opts.mk>
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
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-ffreestanding -I.

LDFLAGS+=	-nostdlib -Wl,-Bsymbolic

NEWVERSWHAT=	"User boot" ${MACHINE_CPUARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

CLEANFILES=	vers.c

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=        -DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/i386
CFLAGS+=	-DBF_DICTSIZE=15000
LIBFICL=	${.OBJDIR}/../ficl/libficl.a
LIBSTAND=	${.OBJDIR}/../libstand/libstand.a
.endif

.if ${MK_ZFS} != "no"
CFLAGS+=	-DUSERBOOT_ZFS_SUPPORT
LIBZFSBOOT=	${.OBJDIR}/../zfs/libzfsboot.a
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.
DPADD+=		${LIBFICL} ${LIBZFSBOOT} ${LIBSTAND} 
LDADD+=		${LIBFICL} ${LIBZFSBOOT} ${LIBSTAND}

.include <bsd.lib.mk>
