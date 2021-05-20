.include <src.opts.mk>

.include "../../Makefile.inc"

LIB=	wpaeapol_supp
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/eapol_supp

SRCS=	eapol_supp_sm.c

CFLAGS+=-DIEEE8021X_EAPOL

.include "../../Makefile.crypto"

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
