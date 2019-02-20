# $FreeBSD$
#

.include <bsd.init.mk>

LIB=			pc98

.PATH:	${BOOTSRC}/i386/libi386

SRCS=	bioscd.c biosdisk.c biosmem.c biospnp.c \
	biospci.c biossmap.c bootinfo.c bootinfo32.c \
	comconsole.c devicename.c elf32_freebsd.c \
	i386_copy.c i386_module.c nullconsole.c pc98_sys.c pxe.c pxetramp.s \
	time.c vidconsole.c
.PATH:	${ZFSSRC}
SRCS+=	devicename_stubs.c
CFLAGS+= -I${ZFSSRC}

BOOT_COMCONSOLE_PORT?= 0x238
CFLAGS+=	-DCOMPORT=${BOOT_COMCONSOLE_PORT}

BOOT_COMCONSOLE_SPEED?= 9600
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

# Include simple terminal emulation (cons25-compatible)
CFLAGS+= -DTERM_EMU

# XXX: make alloca() useable
CFLAGS+= -Dalloca=__builtin_alloca

CFLAGS+=	-I${BOOTSRC}/ficl -I${BOOTSRC}/ficl/i386 \
		-I${LDRSRC} -I${BOOTSRC}/common \
		-I${BOOTSRC}/pc98/btx/lib \
		-I${BOOTSRC}/i386/libi386 \
		-I${SYSDIR}

# Handle FreeBSD specific %b and %D printf format specifiers
CFLAGS+= ${FORMAT_EXTENSIONS}

.include <bsd.lib.mk>
