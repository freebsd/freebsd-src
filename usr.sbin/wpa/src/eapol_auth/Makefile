.include <src.opts.mk>

.include "../../Makefile.inc"

LIB=	wpaeapol_auth
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/eapol_auth

SRCS=	eapol_auth_sm.c \
	eapol_auth_dump.c

.include "../../Makefile.crypto"

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
