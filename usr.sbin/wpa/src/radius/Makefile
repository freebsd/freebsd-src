.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wparadius
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/radius

SRCS=	radius.c \
	radius_client.c \
	radius_das.c \
	radius_server.c

.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
