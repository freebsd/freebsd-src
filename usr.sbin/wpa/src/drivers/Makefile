.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpadrivers
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/drivers

SRCS=	drivers.c \
	driver_common.c \
	driver_wired.c \
	driver_wired_common.c

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
