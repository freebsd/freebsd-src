# $FreeBSD$
#
LIB=			i386
NOPIC=
NOPROFILE=
INTERNALLIB=		true
INTERNALSTATICLIB=	true

SRCS=	aout_freebsd.c biosacpi.c bioscd.c biosdisk.c biosmem.c biospnp.c \
	biospci.c bootinfo.c comconsole.c devicename.c elf_freebsd.c gatea20.c \
	i386_copy.c i386_module.c nullconsole.c pxe.c pxetramp.s \
	time.c vidconsole.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../../contrib/dev/acpica \
		-I${.CURDIR}/../../.. -I.

BOOT_COMCONSOLE_PORT?= 0x3f8
CFLAGS+=	-DCOMPORT=${BOOT_COMCONSOLE_PORT}

BOOT_COMCONSOLE_SPEED?= 9600
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

# the location of libstand
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

# Include simple terminal emulation (cons25-compatible)
CFLAGS+= -DTERM_EMU

# Make "machine" required for all objects
# (based on the more complete case in sys/i386/boot/Makefile.inc)
${SRCS:M*.c:R:S/$/.o/g}: machine

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine

.include <bsd.lib.mk>
