# $FreeBSD$
#
LIB=			i386
INTERNALLIB=

SRCS=	biosacpi.c bioscd.c biosdisk.c biosmem.c biospnp.c \
	biospci.c biossmap.c bootinfo.c bootinfo32.c bootinfo64.c \
	comconsole.c devicename.c elf32_freebsd.c \
	elf64_freebsd.c multiboot.c multiboot_tramp.S \
	i386_copy.c i386_module.c nullconsole.c pxe.c pxetramp.s \
	smbios.c time.c vidconsole.c amd64_tramp.S spinconsole.c
.PATH:	${.CURDIR}/../../zfs
SRCS+=	devicename_stubs.c

# Enable PXE TFTP or NFS support, not both.
.if defined(LOADER_TFTP_SUPPORT)
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.else
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif

BOOT_COMCONSOLE_PORT?= 0x3f8
CFLAGS+=	-DCOMPORT=${BOOT_COMCONSOLE_PORT}

BOOT_COMCONSOLE_SPEED?= 9600
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.if !defined(BOOT_HIDE_SERIAL_NUMBERS)
# Export serial numbers, UUID, and asset tag from loader.
CFLAGS+= -DSMBIOS_SERIAL_NUMBERS
.if defined(BOOT_LITTLE_ENDIAN_UUID)
# Use little-endian UUID format as defined in SMBIOS 2.6.
CFLAGS+= -DSMBIOS_LITTLE_ENDIAN_UUID
.endif
.endif

# Include simple terminal emulation (cons25-compatible)
CFLAGS+= -DTERM_EMU

# XXX: make alloca() useable
CFLAGS+= -Dalloca=__builtin_alloca

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../common \
		-I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../../contrib/dev/acpica/include \
		-I${.CURDIR}/../../.. -I.
# the location of libstand
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Suppress warning from clang for FreeBSD %b and %D formats
CFLAGS+= -fformat-extensions

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.lib.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.amd64_tramp.S=	${CLANG_NO_IAS}
CFLAGS.multiboot_tramp.S=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif
