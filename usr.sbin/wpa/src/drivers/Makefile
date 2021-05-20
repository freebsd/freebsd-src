.include <src.opts.mk>

.include "../../Makefile.inc"

LIB=	wpadrivers
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/drivers

SRCS=	drivers.c \
	driver_bsd.c \
	driver_common.c \
	driver_wired.c \
	driver_wired_common.c

.include "../../Makefile.crypto"

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
