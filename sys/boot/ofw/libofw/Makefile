# $FreeBSD$

LIB=		ofw
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
INTERNALSTATICLIB=	true

SRCS=	devicename.c main.c ofw_copy.c ofw_module.c ofw_disk.c ofw_console.c \
	ofw_time.c ofw_devsearch.c ofw_reboot.c openfirm.c

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -msoft-float \
		-I${.CURDIR}/../../.. -I.

.ifdef(BOOT_BIOSDISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.include <bsd.lib.mk>
