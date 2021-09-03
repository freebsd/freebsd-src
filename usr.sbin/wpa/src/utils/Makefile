.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpautils
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/utils

SRCS=	base64.c \
	bitfield.c \
	crc32.c \
	common.c \
	config.c \
	edit.c \
	eloop.c \
	ip_addr.c \
	os_unix.c \
	uuid.c \
	wpa_debug.c \
	wpabuf.c

CFLAGS+=-DCONFIG_DEBUG_FILE

.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
