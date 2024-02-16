
.include <bsd.init.mk>

LIB=	i386

SRCS+=	amd64_tramp.S
SRCS+=	bio.c
SRCS+=	biosacpi.c
SRCS+=	biosdisk.c
SRCS+=	biosmem.c
SRCS+=	biospci.c
SRCS+=	biospnp.c
SRCS+=	biossmap.c
SRCS+=	bootinfo.c
SRCS+=	bootinfo32.c
SRCS+=	bootinfo64.c
SRCS+=	comconsole.c
SRCS+=	devicename.c
SRCS+=	elf32_freebsd.c
SRCS+=	elf64_freebsd.c
SRCS+=	i386_copy.c
SRCS+=	i386_module.c
SRCS+=	multiboot.c
SRCS+=	multiboot_tramp.S
SRCS+=	nullconsole.c
SRCS+=	pxe.c
SRCS+=	pxetramp.S
SRCS+=	relocater_tramp.S
SRCS+=	spinconsole.c
SRCS+=	time.c
SRCS+=	vbe.c
SRCS+=	vidconsole.c

.PATH:	${SYSDIR}/teken
SRCS+=	teken.c

.include "${BOOTSRC}/veriexec.mk"

BOOT_COMCONSOLE_PORT?= 0x3f8
CFLAGS+=	-DCOMPORT=${BOOT_COMCONSOLE_PORT}

BOOT_COMCONSOLE_SPEED?= 115200
CFLAGS+=	-DCOMSPEED=${BOOT_COMCONSOLE_SPEED}

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS.biosdisk.c+= -DDISK_DEBUG
.endif

# terminal emulation
.if ${BOOT_FRAMEBUFFER_MODE:Uno} == "yes"
CFLAGS.vidconsole.c+= -DFRAMEBUFFER_MODE
.endif
CFLAGS.vidconsole.c+= -I${SRCTOP}/sys/teken -I${SRCTOP}/contrib/pnglite
CFLAGS.teken.c+= -I${SRCTOP}/sys/teken
CFLAGS.bootinfo.c+= -I${SRCTOP}/sys/teken -I${SRCTOP}/contrib/pnglite
CFLAGS.vbe.c+= -I${SRCTOP}/sys/teken -I${SRCTOP}/contrib/pnglite

CFLAGS+=	-I${LDRSRC} -I${BOOTSRC}/i386/common \
		-I${SYSDIR}/contrib/dev/acpica/include

# Handle FreeBSD specific %b and %D printf format specifiers
CFLAGS+= ${FORMAT_EXTENSIONS}

.include <bsd.lib.mk>
