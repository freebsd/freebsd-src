.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpapasn
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/pasn

SRCS=	pasn_common.c \
	pasn_initiator.c

CFLAGS+=	-I${WPA_DISTDIR}/src/pasn -I${WPA_DISTDIR}/src

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
