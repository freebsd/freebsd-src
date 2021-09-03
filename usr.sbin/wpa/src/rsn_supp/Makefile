.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wparsn_supp
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/rsn_supp

SRCS=	pmksa_cache.c \
	wpa_ft.c \
	tdls.c \
	preauth.c \
	wpa.c \
	wpa_ie.c

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
