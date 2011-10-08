# $FreeBSD$

NO_MAN=
WITHOUT_SSP=

.include <bsd.own.mk>

SHLIB_NAME=	userboot.so
NO_CTF=		yes
STRIP=
LIBDIR=		/boot

SRCS=		autoload.c
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
CFLAGS+=	-DLOADER_GPT_SUPPORT -DLOADER_MBR_SUPPORT

LDFLAGS+=	-nostdlib -Wl,-Bsymbolic

NEWVERSWHAT=	"User boot" ${MACHINE_CPUARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

CLEANFILES=	vers.c

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=        -DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/i386
LIBFICL=	${.OBJDIR}/../ficl/libficl.a
LIBSTAND=	${.OBJDIR}/../libstand/libstand.a
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.
DPADD=		${LIBFICL} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBSTAND}

.include <bsd.lib.mk>
